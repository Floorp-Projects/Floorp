/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#undef NDEBUG
#include <assert.h>
#include <cstring>
#include <cstdlib>
#include <cstdio>
#include "elfxx.h"

#define ver "0"
#define elfhack_data ".elfhack.data.v" ver
#define elfhack_text ".elfhack.text.v" ver

#ifndef R_ARM_V4BX
#define R_ARM_V4BX 0x28
#endif
#ifndef R_ARM_CALL
#define R_ARM_CALL 0x1c
#endif
#ifndef R_ARM_JUMP24
#define R_ARM_JUMP24 0x1d
#endif
#ifndef R_ARM_THM_JUMP24
#define R_ARM_THM_JUMP24 0x1e
#endif

char *rundir = nullptr;

template <typename T>
struct wrapped {
    T value;
};

class Elf_Addr_Traits {
public:
    typedef wrapped<Elf32_Addr> Type32;
    typedef wrapped<Elf64_Addr> Type64;

    template <class endian, typename R, typename T>
    static inline void swap(T &t, R &r) {
        r.value = endian::swap(t.value);
    }
};

typedef serializable<Elf_Addr_Traits> Elf_Addr;

class Elf_RelHack_Traits {
public:
    typedef Elf32_Rel Type32;
    typedef Elf32_Rel Type64;

    template <class endian, typename R, typename T>
    static inline void swap(T &t, R &r) {
        r.r_offset = endian::swap(t.r_offset);
        r.r_info = endian::swap(t.r_info);
    }
};

typedef serializable<Elf_RelHack_Traits> Elf_RelHack;

class ElfRelHack_Section: public ElfSection {
public:
    ElfRelHack_Section(Elf_Shdr &s)
    : ElfSection(s, nullptr, nullptr)
    {
        name = elfhack_data;
    };

    void serialize(std::ofstream &file, char ei_class, char ei_data)
    {
        for (std::vector<Elf_RelHack>::iterator i = rels.begin();
             i != rels.end(); ++i)
            (*i).serialize(file, ei_class, ei_data);
    }

    bool isRelocatable() {
        return true;
    }

    void push_back(Elf_RelHack &r) {
        rels.push_back(r);
        shdr.sh_size = rels.size() * shdr.sh_entsize;
    }
private:
    std::vector<Elf_RelHack> rels;
};

class ElfRelHackCode_Section: public ElfSection {
public:
    ElfRelHackCode_Section(Elf_Shdr &s, Elf &e, ElfRelHack_Section &relhack_section,
                           unsigned int init, unsigned int mprotect_cb)
    : ElfSection(s, nullptr, nullptr), parent(e), relhack_section(relhack_section),
      init(init), mprotect_cb(mprotect_cb) {
        std::string file(rundir);
        file += "/inject/";
        switch (parent.getMachine()) {
        case EM_386:
            file += "x86";
            break;
        case EM_X86_64:
            file += "x86_64";
            break;
        case EM_ARM:
            file += "arm";
            break;
        default:
            throw std::runtime_error("unsupported architecture");
        }
        file += ".o";
        std::ifstream inject(file.c_str(), std::ios::in|std::ios::binary);
        elf = new Elf(inject);
        if (elf->getType() != ET_REL)
            throw std::runtime_error("object for injected code is not ET_REL");
        if (elf->getMachine() != parent.getMachine())
            throw std::runtime_error("architecture of object for injected code doesn't match");

        ElfSymtab_Section *symtab = nullptr;

        // Find the symbol table.
        for (ElfSection *section = elf->getSection(1); section != nullptr;
             section = section->getNext()) {
            if (section->getType() == SHT_SYMTAB)
                symtab = (ElfSymtab_Section *) section;
        }
        if (symtab == nullptr)
            throw std::runtime_error("Couldn't find a symbol table for the injected code");

        relro = parent.getSegmentByType(PT_GNU_RELRO);
        align = parent.getSegmentByType(PT_LOAD)->getAlign();

        // Find the init symbol
        entry_point = -1;
        std::string symbol = "init";
        if (!init)
            symbol += "_noinit";
        if (relro)
            symbol += "_relro";
        Elf_SymValue *sym = symtab->lookup(symbol.c_str());
        if (!sym)
            throw std::runtime_error("Couldn't find an 'init' symbol in the injected code");

        entry_point = sym->value.getValue();

        // Get all relevant sections from the injected code object.
        add_code_section(sym->value.getSection());

        // Adjust code sections offsets according to their size
        std::vector<ElfSection *>::iterator c = code.begin();
        (*c)->getShdr().sh_addr = 0;
        for(ElfSection *last = *(c++); c != code.end(); c++) {
            unsigned int addr = last->getShdr().sh_addr + last->getSize();
            if (addr & ((*c)->getAddrAlign() - 1))
                addr = (addr | ((*c)->getAddrAlign() - 1)) + 1;
            (*c)->getShdr().sh_addr = addr;
            // We need to align this section depending on the greater
            // alignment required by code sections.
            if (shdr.sh_addralign < (*c)->getAddrAlign())
                shdr.sh_addralign = (*c)->getAddrAlign();
        }
        shdr.sh_size = code.back()->getAddr() + code.back()->getSize();
        data = new char[shdr.sh_size];
        char *buf = data;
        for (c = code.begin(); c != code.end(); c++) {
            memcpy(buf, (*c)->getData(), (*c)->getSize());
            buf += (*c)->getSize();
        }
        name = elfhack_text;
    }

    ~ElfRelHackCode_Section() {
        delete elf;
    }

    void serialize(std::ofstream &file, char ei_class, char ei_data)
    {
        // Readjust code offsets
        for (std::vector<ElfSection *>::iterator c = code.begin(); c != code.end(); c++)
            (*c)->getShdr().sh_addr += getAddr();

        // Apply relocations
        for (std::vector<ElfSection *>::iterator c = code.begin(); c != code.end(); c++) {
            for (ElfSection *rel = elf->getSection(1); rel != nullptr; rel = rel->getNext())
                if (((rel->getType() == SHT_REL) ||
                     (rel->getType() == SHT_RELA)) &&
                    (rel->getInfo().section == *c)) {
                    if (rel->getType() == SHT_REL)
                        apply_relocations((ElfRel_Section<Elf_Rel> *)rel, *c);
                    else
                        apply_relocations((ElfRel_Section<Elf_Rela> *)rel, *c);
                }
            }

        ElfSection::serialize(file, ei_class, ei_data);
    }

    bool isRelocatable() {
        return true;
    }

    unsigned int getEntryPoint() {
        return entry_point;
    }
private:
    void add_code_section(ElfSection *section)
    {
        if (section) {
            /* Don't add section if it's already been added in the past */
            for (auto s = code.begin(); s != code.end(); ++s) {
                if (section == *s)
                    return;
            }
            code.push_back(section);
            find_code(section);
        }
    }

    /* Look at the relocations associated to the given section to find other
     * sections that it requires */
    void find_code(ElfSection *section)
    {
        for (ElfSection *s = elf->getSection(1); s != nullptr;
             s = s->getNext()) {
            if (((s->getType() == SHT_REL) ||
                 (s->getType() == SHT_RELA)) &&
                (s->getInfo().section == section)) {
                if (s->getType() == SHT_REL)
                    scan_relocs_for_code((ElfRel_Section<Elf_Rel> *)s);
                else
                    scan_relocs_for_code((ElfRel_Section<Elf_Rela> *)s);
            }
        }
    }

    template <typename Rel_Type>
    void scan_relocs_for_code(ElfRel_Section<Rel_Type> *rel)
    {
        ElfSymtab_Section *symtab = (ElfSymtab_Section *)rel->getLink();
        for (auto r = rel->rels.begin(); r != rel->rels.end(); r++) {
            ElfSection *section = symtab->syms[ELF32_R_SYM(r->r_info)].value.getSection();
            add_code_section(section);
        }
    }

    class pc32_relocation {
    public:
        Elf32_Addr operator()(unsigned int base_addr, Elf32_Off offset,
                              Elf32_Word addend, unsigned int addr)
        {
            return addr + addend - offset - base_addr;
        }
    };

    class arm_plt32_relocation {
    public:
        Elf32_Addr operator()(unsigned int base_addr, Elf32_Off offset,
                              Elf32_Word addend, unsigned int addr)
        {
            // We don't care about sign_extend because the only case where this is
            // going to be used only jumps forward.
            Elf32_Addr tmp = (Elf32_Addr) (addr - offset - base_addr) >> 2;
            tmp = (addend + tmp) & 0x00ffffff;
            return (addend & 0xff000000) | tmp;
        }
    };

    class arm_thm_jump24_relocation {
    public:
        Elf32_Addr operator()(unsigned int base_addr, Elf32_Off offset,
                              Elf32_Word addend, unsigned int addr)
        {
            /* Follows description of b.w and bl instructions as per
               ARM Architecture Reference Manual ARM® v7-A and ARM® v7-R edition, A8.6.16
               We limit ourselves to Encoding T4 of b.w and Encoding T1 of bl.
               We don't care about sign_extend because the only case where this is
               going to be used only jumps forward. */
            Elf32_Addr tmp = (Elf32_Addr) (addr - offset - base_addr);
            unsigned int word0 = addend & 0xffff,
                         word1 = addend >> 16;

            /* Encoding T4 of B.W is 10x1 ; Encoding T1 of BL is 11x1. */
            unsigned int type = (word1 & 0xd000) >> 12;
            if (((word0 & 0xf800) != 0xf000) || ((type & 0x9) != 0x9))
                throw std::runtime_error("R_ARM_THM_JUMP24/R_ARM_THM_CALL relocation only supported for B.W <label> and BL <label>");

            /* When the target address points to ARM code, switch a BL to a
             * BLX. This however can't be done with a B.W without adding a
             * trampoline, which is not supported as of now. */
            if ((addr & 0x1) == 0) {
                if (type == 0x9)
                    throw std::runtime_error("R_ARM_THM_JUMP24/R_ARM_THM_CALL relocation only supported for BL <label> when label points to ARM code");
                /* The address of the target is always relative to a 4-bytes
                 * aligned address, so if the address of the BL instruction is
                 * not 4-bytes aligned, adjust for it. */
                if ((base_addr + offset) & 0x2)
                    tmp += 2;
                /* Encoding T2 of BLX is 11x0. */
                type = 0xc;
            }

            unsigned int s = (word0 & (1 << 10)) >> 10;
            unsigned int j1 = (word1 & (1 << 13)) >> 13;
            unsigned int j2 = (word1 & (1 << 11)) >> 11;
            unsigned int i1 = j1 ^ s ? 0 : 1;
            unsigned int i2 = j2 ^ s ? 0 : 1;

            tmp += ((s << 24) | (i1 << 23) | (i2 << 22) | ((word0 & 0x3ff) << 12) | ((word1 & 0x7ff) << 1));

            s = (tmp & (1 << 24)) >> 24;
            j1 = ((tmp & (1 << 23)) >> 23) ^ !s;
            j2 = ((tmp & (1 << 22)) >> 22) ^ !s;

            return 0xf000 | (s << 10) | ((tmp & (0x3ff << 12)) >> 12) |
                   (type << 28) | (j1 << 29) | (j2 << 27) | ((tmp & 0xffe) << 15);
        }
    };

    class gotoff_relocation {
    public:
        Elf32_Addr operator()(unsigned int base_addr, Elf32_Off offset,
                              Elf32_Word addend, unsigned int addr)
        {
            return addr + addend;
        }
    };

    template <class relocation_type>
    void apply_relocation(ElfSection *the_code, char *base, Elf_Rel *r, unsigned int addr)
    {
        relocation_type relocation;
        Elf32_Addr value;
        memcpy(&value, base + r->r_offset, 4);
        value = relocation(the_code->getAddr(), r->r_offset, value, addr);
        memcpy(base + r->r_offset, &value, 4);
    }

    template <class relocation_type>
    void apply_relocation(ElfSection *the_code, char *base, Elf_Rela *r, unsigned int addr)
    {
        relocation_type relocation;
        Elf32_Addr value = relocation(the_code->getAddr(), r->r_offset, r->r_addend, addr);
        memcpy(base + r->r_offset, &value, 4);
    }

    template <typename Rel_Type>
    void apply_relocations(ElfRel_Section<Rel_Type> *rel, ElfSection *the_code)
    {
        assert(rel->getType() == Rel_Type::sh_type);
        char *buf = data + (the_code->getAddr() - code.front()->getAddr());
        // TODO: various checks on the sections
        ElfSymtab_Section *symtab = (ElfSymtab_Section *)rel->getLink();
        for (typename std::vector<Rel_Type>::iterator r = rel->rels.begin(); r != rel->rels.end(); r++) {
            // TODO: various checks on the symbol
            const char *name = symtab->syms[ELF32_R_SYM(r->r_info)].name;
            unsigned int addr;
            if (symtab->syms[ELF32_R_SYM(r->r_info)].value.getSection() == nullptr) {
                if (strcmp(name, "relhack") == 0) {
                    addr = relhack_section.getAddr();
                } else if (strcmp(name, "elf_header") == 0) {
                    // TODO: change this ungly hack to something better
                    ElfSection *ehdr = parent.getSection(1)->getPrevious()->getPrevious();
                    addr = ehdr->getAddr();
                } else if (strcmp(name, "original_init") == 0) {
                    addr = init;
                } else if (relro && strcmp(name, "mprotect_cb") == 0) {
                    addr = mprotect_cb;
                } else if (relro && strcmp(name, "relro_start") == 0) {
                    // Align relro segment start to the start of the page it starts in.
                    addr = relro->getAddr() & ~(align - 1);
                    // Align relro segment end to the start of the page it ends into.
                } else if (relro && strcmp(name, "relro_end") == 0) {
                    addr = (relro->getAddr() + relro->getMemSize()) & ~(align - 1);
                } else if (strcmp(name, "_GLOBAL_OFFSET_TABLE_") == 0) {
                    // We actually don't need a GOT, but need it as a reference for
                    // GOTOFF relocations. We'll just use the start of the ELF file
                    addr = 0;
                } else if (strcmp(name, "") == 0) {
                    // This is for R_ARM_V4BX, until we find something better
                    addr = -1;
                } else {
                    throw std::runtime_error("Unsupported symbol in relocation");
                }
            } else {
                ElfSection *section = symtab->syms[ELF32_R_SYM(r->r_info)].value.getSection();
                assert((section->getType() == SHT_PROGBITS) && (section->getFlags() & SHF_EXECINSTR));
                addr = symtab->syms[ELF32_R_SYM(r->r_info)].value.getValue();
            }
            // Do the relocation
#define REL(machine, type) (EM_ ## machine | (R_ ## machine ## _ ## type << 8))
            switch (elf->getMachine() | (ELF32_R_TYPE(r->r_info) << 8)) {
            case REL(X86_64, PC32):
            case REL(386, PC32):
            case REL(386, GOTPC):
            case REL(ARM, GOTPC):
            case REL(ARM, REL32):
                apply_relocation<pc32_relocation>(the_code, buf, &*r, addr);
                break;
            case REL(ARM, CALL):
            case REL(ARM, JUMP24):
            case REL(ARM, PLT32):
                apply_relocation<arm_plt32_relocation>(the_code, buf, &*r, addr);
                break;
            case REL(ARM, THM_PC22 /* THM_CALL */):
            case REL(ARM, THM_JUMP24):
                apply_relocation<arm_thm_jump24_relocation>(the_code, buf, &*r, addr);
                break;
            case REL(386, GOTOFF):
            case REL(ARM, GOTOFF):
                apply_relocation<gotoff_relocation>(the_code, buf, &*r, addr);
                break;
            case REL(ARM, V4BX):
                // Ignore R_ARM_V4BX relocations
                break;
            default:
                throw std::runtime_error("Unsupported relocation type");
            }
        }
    }

    Elf *elf, &parent;
    ElfRelHack_Section &relhack_section;
    std::vector<ElfSection *> code;
    unsigned int init;
    unsigned int mprotect_cb;
    int entry_point;
    ElfSegment *relro;
    unsigned int align;
};

unsigned int get_addend(Elf_Rel *rel, Elf *elf) {
    ElfLocation loc(rel->r_offset, elf);
    Elf_Addr addr(loc.getBuffer(), Elf_Addr::size(elf->getClass()), elf->getClass(), elf->getData());
    return addr.value;
}

unsigned int get_addend(Elf_Rela *rel, Elf *elf) {
    return rel->r_addend;
}

void set_relative_reloc(Elf_Rel *rel, Elf *elf, unsigned int value) {
    ElfLocation loc(rel->r_offset, elf);
    Elf_Addr addr;
    addr.value = value;
    addr.serialize(const_cast<char *>(loc.getBuffer()), Elf_Addr::size(elf->getClass()), elf->getClass(), elf->getData());
}

void set_relative_reloc(Elf_Rela *rel, Elf *elf, unsigned int value) {
    // ld puts the value of relocated relocations both in the addend and
    // at r_offset. For consistency, keep it that way.
    set_relative_reloc((Elf_Rel *)rel, elf, value);
    rel->r_addend = value;
}

void maybe_split_segment(Elf *elf, ElfSegment *segment, bool fill)
{
    std::list<ElfSection *>::iterator it = segment->begin();
    for (ElfSection *last = *(it++); it != segment->end(); last = *(it++)) {
        // When two consecutive non-SHT_NOBITS sections are apart by more
        // than the alignment of the section, the second can be moved closer
        // to the first, but this requires the segment to be split.
        if (((*it)->getType() != SHT_NOBITS) && (last->getType() != SHT_NOBITS) &&
            ((*it)->getOffset() - last->getOffset() - last->getSize() > segment->getAlign())) {
            // Probably very wrong.
            Elf_Phdr phdr;
            phdr.p_type = PT_LOAD;
            phdr.p_vaddr = 0;
            phdr.p_paddr = phdr.p_vaddr + segment->getVPDiff();
            phdr.p_flags = segment->getFlags();
            phdr.p_align = segment->getAlign();
            phdr.p_filesz = (unsigned int)-1;
            phdr.p_memsz = (unsigned int)-1;
            ElfSegment *newSegment = new ElfSegment(&phdr);
            elf->insertSegmentAfter(segment, newSegment);
            ElfSection *section = *it;
            for (; it != segment->end(); ++it) {
                newSegment->addSection(*it);
            }
            for (it = newSegment->begin(); it != newSegment->end(); it++) {
                segment->removeSection(*it);
            }
            // Fill the virtual address space gap left between the two PT_LOADs
            // with a new PT_LOAD with no permissions. This avoids the linker
            // (especially bionic's) filling the gap with anonymous memory,
            // which breakpad doesn't like.
            // /!\ running strip on a elfhacked binary will break this filler
            // PT_LOAD.
            if (!fill)
                break;
            // Insert dummy segment to normalize the entire Elf with the header
            // sizes adjusted, before inserting a filler segment.
            {
              memset(&phdr, 0, sizeof(phdr));
              ElfSegment dummySegment(&phdr);
              elf->insertSegmentAfter(segment, &dummySegment);
              elf->normalize();
              elf->removeSegment(&dummySegment);
            }
            ElfSection *previous = section->getPrevious();
            phdr.p_type = PT_LOAD;
            phdr.p_vaddr = (previous->getAddr() + previous->getSize() + segment->getAlign() - 1) & ~(segment->getAlign() - 1);
            phdr.p_paddr = phdr.p_vaddr + segment->getVPDiff();
            phdr.p_flags = 0;
            phdr.p_align = 0;
            phdr.p_filesz = (section->getAddr() & ~(newSegment->getAlign() - 1)) - phdr.p_vaddr;
            phdr.p_memsz = phdr.p_filesz;
            if (phdr.p_filesz) {
                newSegment = new ElfSegment(&phdr);
                assert(newSegment->isElfHackFillerSegment());
                elf->insertSegmentAfter(segment, newSegment);
            } else {
                elf->normalize();
            }
            break;
        }
    }
}

template <typename Rel_Type>
int do_relocation_section(Elf *elf, unsigned int rel_type, unsigned int rel_type2, bool force, bool fill)
{
    ElfDynamic_Section *dyn = elf->getDynSection();
    if (dyn == nullptr) {
        fprintf(stderr, "Couldn't find SHT_DYNAMIC section\n");
        return -1;
    }

    ElfRel_Section<Rel_Type> *section = (ElfRel_Section<Rel_Type> *)dyn->getSectionForType(Rel_Type::d_tag);
    if (section == nullptr) {
        fprintf(stderr, "No relocations\n");
        return -1;
    }
    assert(section->getType() == Rel_Type::sh_type);

    Elf32_Shdr relhack32_section =
        { 0, SHT_PROGBITS, SHF_ALLOC, 0, (Elf32_Off)-1, 0, SHN_UNDEF, 0,
          Elf_RelHack::size(elf->getClass()), Elf_RelHack::size(elf->getClass()) }; // TODO: sh_addralign should be an alignment, not size
    Elf32_Shdr relhackcode32_section =
        { 0, SHT_PROGBITS, SHF_ALLOC | SHF_EXECINSTR, 0, (Elf32_Off)-1, 0,
          SHN_UNDEF, 0, 1, 0 };

    unsigned int entry_sz = Elf_Addr::size(elf->getClass());

    // The injected code needs to be executed before any init code in the
    // binary. There are three possible cases:
    // - The binary has no init code at all. In this case, we will add a
    //   DT_INIT entry pointing to the injected code.
    // - The binary has a DT_INIT entry. In this case, we will interpose:
    //   we change DT_INIT to point to the injected code, and have the
    //   injected code call the original DT_INIT entry point.
    // - The binary has no DT_INIT entry, but has a DT_INIT_ARRAY. In this
    //   case, we interpose as well, by replacing the first entry in the
    //   array to point to the injected code, and have the injected code
    //   call the original first entry.
    // The binary may have .ctors instead of DT_INIT_ARRAY, for its init
    // functions, but this falls into the second case above, since .ctors
    // are actually run by DT_INIT code.
    ElfValue *value = dyn->getValueForType(DT_INIT);
    unsigned int original_init = value ? value->getValue() : 0;
    ElfSection *init_array = nullptr;
    if (!value || !value->getValue()) {
        value = dyn->getValueForType(DT_INIT_ARRAYSZ);
        if (value && value->getValue() >= entry_sz)
            init_array = dyn->getSectionForType(DT_INIT_ARRAY);
    }

    Elf_Shdr relhack_section(relhack32_section);
    Elf_Shdr relhackcode_section(relhackcode32_section);
    ElfRelHack_Section *relhack = new ElfRelHack_Section(relhack_section);

    ElfSymtab_Section *symtab = (ElfSymtab_Section *) section->getLink();
    Elf_SymValue *sym = symtab->lookup("__cxa_pure_virtual");

    std::vector<Rel_Type> new_rels;
    Elf_RelHack relhack_entry;
    relhack_entry.r_offset = relhack_entry.r_info = 0;
    size_t init_array_reloc = 0;
    for (typename std::vector<Rel_Type>::iterator i = section->rels.begin();
         i != section->rels.end(); i++) {
        // We don't need to keep R_*_NONE relocations
        if (!ELF32_R_TYPE(i->r_info))
            continue;
        ElfLocation loc(i->r_offset, elf);
        // __cxa_pure_virtual is a function used in vtables to point at pure
        // virtual methods. The __cxa_pure_virtual function usually abort()s.
        // These functions are however normally never called. In the case
        // where they would, jumping to the null address instead of calling
        // __cxa_pure_virtual is going to work just as well. So we can remove
        // relocations for the __cxa_pure_virtual symbol and null out the
        // content at the offset pointed by the relocation.
        if (sym) {
            if (sym->defined) {
                // If we are statically linked to libstdc++, the
                // __cxa_pure_virtual symbol is defined in our lib, and we
                // have relative relocations (rel_type) for it.
                if (ELF32_R_TYPE(i->r_info) == rel_type) {
                    Elf_Addr addr(loc.getBuffer(), entry_sz, elf->getClass(), elf->getData());
                    if (addr.value == sym->value.getValue()) {
                        memset((char *)loc.getBuffer(), 0, entry_sz);
                        continue;
                    }
                }
            } else {
                // If we are dynamically linked to libstdc++, the
                // __cxa_pure_virtual symbol is undefined in our lib, and we
                // have absolute relocations (rel_type2) for it.
                if ((ELF32_R_TYPE(i->r_info) == rel_type2) &&
                    (sym == &symtab->syms[ELF32_R_SYM(i->r_info)])) {
                    memset((char *)loc.getBuffer(), 0, entry_sz);
                    continue;
                }
            }
        }
        // Keep track of the relocation associated with the first init_array entry.
        if (init_array && i->r_offset == init_array->getAddr()) {
            if (init_array_reloc) {
                fprintf(stderr, "Found multiple relocations for the first init_array entry. Skipping\n");
                return -1;
            }
            new_rels.push_back(*i);
            init_array_reloc = new_rels.size();
        } else if (!(loc.getSection()->getFlags() & SHF_WRITE) || (ELF32_R_TYPE(i->r_info) != rel_type)) {
            // Don't pack relocations happening in non writable sections.
            // Our injected code is likely not to be allowed to write there.
            new_rels.push_back(*i);
        } else {
            // With Elf_Rel, the value pointed by the relocation offset is the addend.
            // With Elf_Rela, the addend is in the relocation entry, but the elfhacked
            // relocation info doesn't contain it. Elfhack relies on the value pointed
            // by the relocation offset to also contain the addend. Which is true with
            // BFD ld and gold, but not lld, which leaves that nulled out. So if that
            // value is nulled out, we update it to the addend.
            Elf_Addr addr(loc.getBuffer(), entry_sz, elf->getClass(), elf->getData());
            unsigned int addend = get_addend(&*i, elf);
            if (addr.value == 0) {
                addr.value = addend;
                addr.serialize(const_cast<char*>(loc.getBuffer()), entry_sz, elf->getClass(), elf->getData());
            } else if (addr.value != addend) {
                fprintf(stderr, "Relocation addend inconsistent with content. Skipping\n");
                return -1;
            }
            if (i->r_offset == relhack_entry.r_offset + relhack_entry.r_info * entry_sz) {
                relhack_entry.r_info++;
            } else {
                if (relhack_entry.r_offset)
                    relhack->push_back(relhack_entry);
                relhack_entry.r_offset = i->r_offset;
                relhack_entry.r_info = 1;
            }
        }
    }
    if (relhack_entry.r_offset)
        relhack->push_back(relhack_entry);
    // Last entry must be nullptr
    relhack_entry.r_offset = relhack_entry.r_info = 0;
    relhack->push_back(relhack_entry);

    if (init_array && !init_array_reloc) {
        // Some linkers create a DT_INIT_ARRAY section that, for all purposes,
        // is empty: it only contains 0x0 or 0xffffffff pointers with no relocations.
        const size_t zero = 0;
        const size_t all = SIZE_MAX;
        const char *data = init_array->getData();
        size_t length = Elf_Addr::size(elf->getClass());
        bool empty = true;
        for (size_t off = 0; off < init_array->getSize(); off += length) {
            if (memcmp(data + off, &zero, length) &&
                memcmp(data + off, &all, length)) {
                empty = false;
                break;
            }
        }
	// If we encounter such an empty DT_INIT_ARRAY section, we add a
	// relocation for its first entry to point to our init. Code further
	// below will take care of actually setting the right r_info and
	// r_addend for the relocation, as if we had a normal DT_INIT_ARRAY
	// section.
        if (empty) {
            new_rels.emplace_back();
            init_array_reloc = new_rels.size();
            Rel_Type *rel = &new_rels[init_array_reloc - 1];
            rel->r_offset = init_array->getAddr();
        } else {
            fprintf(stderr, "Didn't find relocation for DT_INIT_ARRAY's first entry. Skipping\n");
            return -1;
        }
    } else if (init_array) {
        Rel_Type *rel = &new_rels[init_array_reloc - 1];
        unsigned int addend = get_addend(rel, elf);
        // Use relocated value of DT_INIT_ARRAY's first entry for the
        // function to be called by the injected code.
        if (ELF32_R_TYPE(rel->r_info) == rel_type) {
            original_init = addend;
        } else if (ELF32_R_TYPE(rel->r_info) == rel_type2) {
            ElfSymtab_Section *symtab = (ElfSymtab_Section *)section->getLink();
            original_init = symtab->syms[ELF32_R_SYM(rel->r_info)].value.getValue() + addend;
        } else {
            fprintf(stderr, "Unsupported relocation type for DT_INIT_ARRAY's first entry. Skipping\n");
            return -1;
        }
    }

    unsigned int mprotect_cb = 0;
    // If there is a relro segment, our injected code will run after the linker sets the
    // corresponding pages read-only. We need to make our code change that to read-write
    // before applying relocations, which means it needs to call mprotect.
    // To do that, we need to find a reference to the mprotect symbol. In case the library
    // already has one, we use that, but otherwise, we add the symbol.
    // Then the injected code needs to be able to call the corresponding function, which
    // means it needs access to a pointer to it. We get such a pointer by making the linker
    // apply a relocation for the symbol at an address our code can read.
    // The problem here is that there is not much relocated space where we can put such a
    // pointer, so we abuse the bss section temporarily (it will be restored to a null
    // value before any code can actually use it)
    if (elf->getSegmentByType(PT_GNU_RELRO)) {
        Elf_SymValue *mprotect = symtab->lookup("mprotect", STT(FUNC));
        if (!mprotect) {
            symtab->syms.emplace_back();
            mprotect = &symtab->syms.back();
            symtab->grow(symtab->syms.size() * symtab->getEntSize());
            mprotect->name = ((ElfStrtab_Section *)symtab->getLink())->getStr("mprotect");
            mprotect->info = ELF32_ST_INFO(STB_GLOBAL, STT_FUNC);
            mprotect->other = STV_DEFAULT;
            new (&mprotect->value) ElfLocation(nullptr, 0, ElfLocation::ABSOLUTE);
            mprotect->size = 0;
            mprotect->defined = false;

            // The DT_VERSYM data (in the .gnu.version section) has the same number of
            // entries as the symbols table. Since we added one entry there, we need to
            // add one entry here. Zeroes in the extra data means no version for that
            // symbol, which is the simplest thing to do.
            ElfSection *gnu_versym = dyn->getSectionForType(DT_VERSYM);
            if (gnu_versym) {
               gnu_versym->grow(gnu_versym->getSize() + gnu_versym->getEntSize());
            }
        }

        // Add a relocation for the mprotect symbol.
        new_rels.emplace_back();
        Rel_Type &rel = new_rels.back();
        memset(&rel, 0, sizeof(rel));
        rel.r_info = ELF32_R_INFO(std::distance(symtab->syms.begin(), std::vector<Elf_SymValue>::iterator(mprotect)), rel_type2);

        // Find the beginning of the bss section, and use an aligned location in there
        // for the relocation.
        for (ElfSection *s = elf->getSection(1); s != nullptr; s = s->getNext()) {
            if (s->getType() != SHT_NOBITS || (s->getFlags() & (SHF_TLS | SHF_WRITE)) != SHF_WRITE) {
                continue;
            }
            size_t ptr_size = Elf_Addr::size(elf->getClass());
            size_t usable_start = (s->getAddr() + ptr_size - 1) & ~(ptr_size - 1);
            size_t usable_end = (s->getAddr() + s->getSize()) & ~(ptr_size - 1);
            if (usable_end - usable_start >= ptr_size) {
                mprotect_cb = rel.r_offset = usable_start;
                break;
            }
        }

        if (mprotect_cb == 0) {
            fprintf(stderr, "Couldn't find .bss. Skipping\n");
            return -1;
        }
    }

    section->rels.assign(new_rels.begin(), new_rels.end());
    section->shrink(new_rels.size() * section->getEntSize());

    ElfRelHackCode_Section *relhackcode = new ElfRelHackCode_Section(relhackcode_section, *elf, *relhack, original_init, mprotect_cb);
    // Find the first executable section, and insert the relhack code before
    // that. The relhack data is inserted between .rel.dyn and .rel.plt.
    ElfSection *first_executable = nullptr;
    for (ElfSection *s = elf->getSection(1); s != nullptr;
         s = s->getNext()) {
        if (s->getFlags() & SHF_EXECINSTR) {
            first_executable = s;
            break;
        }
    }

    if (!first_executable) {
        fprintf(stderr, "Couldn't find executable section. Skipping\n");
        return -1;
    }

    unsigned int old_exec = first_executable->getOffset();

    relhack->insertBefore(section);
    relhackcode->insertBefore(first_executable);

    // Trying to get first_executable->getOffset() now may throw if the new
    // layout would require it to move, so we look at the end of the relhack
    // code section instead, comparing it to where the first executable
    // section used to start.
    if (relhackcode->getOffset() + relhackcode->getSize() >= old_exec) {
        fprintf(stderr, "No gain. Skipping\n");
        return -1;
    }

    // Adjust PT_LOAD segments
    for (ElfSegment *segment = elf->getSegmentByType(PT_LOAD); segment;
         segment = elf->getSegmentByType(PT_LOAD, segment)) {
        maybe_split_segment(elf, segment, fill);
    }

    // Ensure Elf sections will be at their final location.
    elf->normalize();
    ElfLocation *init = new ElfLocation(relhackcode, relhackcode->getEntryPoint());
    if (init_array) {
        // Adjust the first DT_INIT_ARRAY entry to point at the injected code
        // by transforming its relocation into a relative one pointing to the
        // address of the injected code.
        Rel_Type *rel = &section->rels[init_array_reloc - 1];
        rel->r_info = ELF32_R_INFO(0, rel_type); // Set as a relative relocation
        set_relative_reloc(&section->rels[init_array_reloc - 1], elf, init->getValue());
    } else if (!dyn->setValueForType(DT_INIT, init)) {
        fprintf(stderr, "Can't grow .dynamic section to set DT_INIT. Skipping\n");
        return -1;
    }
    // TODO: adjust the value according to the remaining number of relative relocations
    if (dyn->getValueForType(Rel_Type::d_tag_count))
        dyn->setValueForType(Rel_Type::d_tag_count, new ElfPlainValue(0));

    return 0;
}

static inline int backup_file(const char *name)
{
    std::string fname(name);
    fname += ".bak";
    return rename(name, fname.c_str());
}

void do_file(const char *name, bool backup = false, bool force = false, bool fill = false)
{
    std::ifstream file(name, std::ios::in|std::ios::binary);
    Elf elf(file);
    unsigned int size = elf.getSize();
    fprintf(stderr, "%s: ", name);
    if (elf.getType() != ET_DYN) {
        fprintf(stderr, "Not a shared object. Skipping\n");
        return;
    }

    for (ElfSection *section = elf.getSection(1); section != nullptr;
         section = section->getNext()) {
        if (section->getName() &&
            (strncmp(section->getName(), ".elfhack.", 9) == 0)) {
            fprintf(stderr, "Already elfhacked. Skipping\n");
            return;
        }
    }

    int exit = -1;
    switch (elf.getMachine()) {
    case EM_386:
        exit = do_relocation_section<Elf_Rel>(&elf, R_386_RELATIVE, R_386_32, force, fill);
        break;
    case EM_X86_64:
        exit = do_relocation_section<Elf_Rela>(&elf, R_X86_64_RELATIVE, R_X86_64_64, force, fill);
        break;
    case EM_ARM:
        exit = do_relocation_section<Elf_Rel>(&elf, R_ARM_RELATIVE, R_ARM_ABS32, force, fill);
        break;
    }
    if (exit == 0) {
        if (!force && (elf.getSize() >= size)) {
            fprintf(stderr, "No gain. Skipping\n");
        } else if (backup && backup_file(name) != 0) {
            fprintf(stderr, "Couln't create backup file\n");
        } else {
            std::ofstream ofile(name, std::ios::out|std::ios::binary|std::ios::trunc);
            elf.write(ofile);
            fprintf(stderr, "Reduced by %d bytes\n", size - elf.getSize());
        }
    }
}

void undo_file(const char *name, bool backup = false)
{
    std::ifstream file(name, std::ios::in|std::ios::binary);
    Elf elf(file);
    unsigned int size = elf.getSize();
    fprintf(stderr, "%s: ", name);
    if (elf.getType() != ET_DYN) {
        fprintf(stderr, "Not a shared object. Skipping\n");
        return;
    }

    ElfSection *data = nullptr, *text = nullptr;
    for (ElfSection *section = elf.getSection(1); section != nullptr;
         section = section->getNext()) {
        if (section->getName() &&
            (strcmp(section->getName(), elfhack_data) == 0))
            data = section;
        if (section->getName() &&
            (strcmp(section->getName(), elfhack_text) == 0))
            text = section;
    }

    if (!data || !text) {
        fprintf(stderr, "Not elfhacked. Skipping\n");
        return;
    }

    ElfSegment *first = data->getSegmentByType(PT_LOAD);
    ElfSegment *second = text->getSegmentByType(PT_LOAD);
    if (first != second) {
        fprintf(stderr, elfhack_data " and " elfhack_text " not in the same segment. Skipping\n");
        return;
    }
    second = elf.getSegmentByType(PT_LOAD, first);
    ElfSegment *filler = nullptr;
    // If the second PT_LOAD is a filler from elfhack --fill, check the third.
    if (second->isElfHackFillerSegment()) {
        filler = second;
        second = elf.getSegmentByType(PT_LOAD, filler);
    }
    if (second->getFlags() != first->getFlags()) {
        fprintf(stderr, "Couldn't identify elfhacked PT_LOAD segments. Skipping\n");
        return;
    }
    // Move sections from the second PT_LOAD to the first, and remove the
    // second PT_LOAD segment.
    for (std::list<ElfSection *>::iterator section = second->begin();
         section != second->end(); ++section)
        first->addSection(*section);

    elf.removeSegment(second);
    if (filler)
        elf.removeSegment(filler);

    if (backup && backup_file(name) != 0) {
        fprintf(stderr, "Couln't create backup file\n");
    } else {
        std::ofstream ofile(name, std::ios::out|std::ios::binary|std::ios::trunc);
        elf.write(ofile);
        fprintf(stderr, "Grown by %d bytes\n", elf.getSize() - size);
    }
}

int main(int argc, char *argv[])
{
    int arg;
    bool backup = false;
    bool force = false;
    bool revert = false;
    bool fill = false;
    char *lastSlash = rindex(argv[0], '/');
    if (lastSlash != nullptr)
        rundir = strndup(argv[0], lastSlash - argv[0]);
    for (arg = 1; arg < argc; arg++) {
        if (strcmp(argv[arg], "-f") == 0)
            force = true;
        else if (strcmp(argv[arg], "-b") == 0)
            backup = true;
        else if (strcmp(argv[arg], "-r") == 0)
            revert = true;
        else if (strcmp(argv[arg], "--fill") == 0)
            fill = true;
        else if (revert) {
            undo_file(argv[arg], backup);
        } else
            do_file(argv[arg], backup, force, fill);
    }

    free(rundir);
    return 0;
}
