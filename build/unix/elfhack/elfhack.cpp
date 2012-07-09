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
#ifndef R_ARM_THM_JUMP24
#define R_ARM_THM_JUMP24 0x1e
#endif

char *rundir = NULL;

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
    : ElfSection(s, NULL, NULL)
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
    ElfRelHackCode_Section(Elf_Shdr &s, Elf &e)
    : ElfSection(s, NULL, NULL), parent(e) {
        std::string file(rundir);
        init = parent.getDynSection()->getSectionForType(DT_INIT);
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
        if (init == NULL)
            file += "-noinit";
        file += ".o";
        std::ifstream inject(file.c_str(), std::ios::in|std::ios::binary);
        elf = new Elf(inject);
        if (elf->getType() != ET_REL)
            throw std::runtime_error("object for injected code is not ET_REL");
        if (elf->getMachine() != parent.getMachine())
            throw std::runtime_error("architecture of object for injected code doesn't match");

        ElfSymtab_Section *symtab = NULL;

        // Get all executable sections from the injected code object.
        // Most of the time, there will only be one for the init function,
        // but on e.g. x86, there is a separate section for
        // __i686.get_pc_thunk.$reg
        // Find the symbol table at the same time.
        for (ElfSection *section = elf->getSection(1); section != NULL;
             section = section->getNext()) {
            if ((section->getType() == SHT_PROGBITS) &&
                (section->getFlags() & SHF_EXECINSTR)) {
                code.push_back(section);
                // We need to align this section depending on the greater
                // alignment required by code sections.
                if (shdr.sh_addralign < section->getAddrAlign())
                    shdr.sh_addralign = section->getAddrAlign();
            } else if (section->getType() == SHT_SYMTAB) {
                symtab = (ElfSymtab_Section *) section;
            }
        }
        assert(code.size() != 0);
        if (symtab == NULL)
            throw std::runtime_error("Couldn't find a symbol table for the injected code");

        // Find the init symbol
        entry_point = -1;
        int shndx = 0;
        Elf_SymValue *sym = symtab->lookup("init");
        if (sym) {
            entry_point = sym->value.getValue();
            shndx = sym->value.getSection()->getIndex();
        } else
            throw std::runtime_error("Couldn't find an 'init' symbol in the injected code");

        // Adjust code sections offsets according to their size
        std::vector<ElfSection *>::iterator c = code.begin();
        (*c)->getShdr().sh_addr = 0;
        for(ElfSection *last = *(c++); c != code.end(); c++) {
            unsigned int addr = last->getShdr().sh_addr + last->getSize();
            if (addr & ((*c)->getAddrAlign() - 1))
                addr = (addr | ((*c)->getAddrAlign() - 1)) + 1;
            (*c)->getShdr().sh_addr = addr;
        }
        shdr.sh_size = code.back()->getAddr() + code.back()->getSize();
        data = new char[shdr.sh_size];
        char *buf = data;
        for (c = code.begin(); c != code.end(); c++) {
            memcpy(buf, (*c)->getData(), (*c)->getSize());
            buf += (*c)->getSize();
            if ((*c)->getIndex() < shndx)
                entry_point += (*c)->getSize();
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
        for (ElfSection *rel = elf->getSection(1); rel != NULL; rel = rel->getNext())
            if ((rel->getType() == SHT_REL) || (rel->getType() == SHT_RELA)) {
                ElfSection *section = rel->getInfo().section;
                if ((section->getType() == SHT_PROGBITS) && (section->getFlags() & SHF_EXECINSTR)) {
                    if (rel->getType() == SHT_REL)
                        apply_relocations((ElfRel_Section<Elf_Rel> *)rel, section);
                    else
                        apply_relocations((ElfRel_Section<Elf_Rela> *)rel, section);
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
            /* Follows description of b.w instructions as per
               ARM Architecture Reference Manual ARM® v7-A and ARM® v7-R edition, A8.6.16
               We limit ourselves to Encoding T3.
               We don't care about sign_extend because the only case where this is
               going to be used only jumps forward. */
            Elf32_Addr tmp = (Elf32_Addr) (addr - offset - base_addr);
            unsigned int word0 = addend & 0xffff,
                         word1 = addend >> 16;

            if (((word0 & 0xf800) != 0xf000) || ((word1 & 0xd000) != 0x9000))
                throw std::runtime_error("R_ARM_THM_JUMP24 relocation only supported for B.W <label>");

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
                   (0x9000 << 16) | (j1 << 29) | (j2 << 27) | ((tmp & 0xffe) << 15);
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
            if (symtab->syms[ELF32_R_SYM(r->r_info)].value.getSection() == NULL) {
                if (strcmp(name, "relhack") == 0) {
                    addr = getNext()->getAddr();
                } else if (strcmp(name, "elf_header") == 0) {
                    // TODO: change this ungly hack to something better
                    ElfSection *ehdr = parent.getSection(1)->getPrevious()->getPrevious();
                    addr = ehdr->getAddr();
                } else if (strcmp(name, "original_init") == 0) {
                    addr = init->getAddr();
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
            case REL(ARM, PLT32):
                apply_relocation<arm_plt32_relocation>(the_code, buf, &*r, addr);
                break;
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
    std::vector<ElfSection *> code;
    ElfSection *init;
    int entry_point;
};

template <typename Rel_Type>
int do_relocation_section(Elf *elf, unsigned int rel_type, unsigned int rel_type2)
{
    ElfDynamic_Section *dyn = elf->getDynSection();
    if (dyn ==NULL) {
        fprintf(stderr, "Couldn't find SHT_DYNAMIC section\n");
        return -1;
    }

    ElfSegment *relro = elf->getSegmentByType(PT_GNU_RELRO);

    ElfRel_Section<Rel_Type> *section = (ElfRel_Section<Rel_Type> *)dyn->getSectionForType(Rel_Type::d_tag);
    assert(section->getType() == Rel_Type::sh_type);

    Elf32_Shdr relhack32_section =
        { 0, SHT_PROGBITS, SHF_ALLOC, 0, (Elf32_Off)-1, 0, SHN_UNDEF, 0,
          Elf_RelHack::size(elf->getClass()), Elf_RelHack::size(elf->getClass()) }; // TODO: sh_addralign should be an alignment, not size
    Elf32_Shdr relhackcode32_section =
        { 0, SHT_PROGBITS, SHF_ALLOC | SHF_EXECINSTR, 0, (Elf32_Off)-1, 0,
          SHN_UNDEF, 0, 1, 0 };
    Elf_Shdr relhack_section(relhack32_section);
    Elf_Shdr relhackcode_section(relhackcode32_section);
    ElfRelHack_Section *relhack = new ElfRelHack_Section(relhack_section);
    ElfRelHackCode_Section *relhackcode = new ElfRelHackCode_Section(relhackcode_section, *elf);

    ElfSymtab_Section *symtab = (ElfSymtab_Section *) section->getLink();
    Elf_SymValue *sym = symtab->lookup("__cxa_pure_virtual");

    std::vector<Rel_Type> new_rels;
    Elf_RelHack relhack_entry;
    relhack_entry.r_offset = relhack_entry.r_info = 0;
    int entry_sz = (elf->getClass() == ELFCLASS32) ? 4 : 8;
    for (typename std::vector<Rel_Type>::iterator i = section->rels.begin();
         i != section->rels.end(); i++) {
        // We don't need to keep R_*_NONE relocations
        if (!ELF32_R_TYPE(i->r_info))
            continue;
        ElfSection *section = elf->getSectionAt(i->r_offset);
        // __cxa_pure_virtual is a function used in vtables to point at pure
        // virtual methods. The __cxa_pure_virtual function usually abort()s.
        // These functions are however normally never called. In the case
        // where they would, jumping to the NULL address instead of calling
        // __cxa_pure_virtual is going to work just as well. So we can remove
        // relocations for the __cxa_pure_virtual symbol and NULL out the
        // content at the offset pointed by the relocation.
        if (sym) {
            if (sym->defined) {
                // If we are statically linked to libstdc++, the
                // __cxa_pure_virtual symbol is defined in our lib, and we
                // have relative relocations (rel_type) for it.
                if (ELF32_R_TYPE(i->r_info) == rel_type) {
                    serializable<Elf_Addr_Traits> addr(&section->getData()[i->r_offset - section->getAddr()], entry_sz, elf->getClass(), elf->getData());
                    if (addr.value == sym->value.getValue()) {
                        memset((char *)&section->getData()[i->r_offset - section->getAddr()], 0, entry_sz);
                        continue;
                    }
                }
            } else {
                // If we are dynamically linked to libstdc++, the
                // __cxa_pure_virtual symbol is undefined in our lib, and we
                // have absolute relocations (rel_type2) for it.
                if ((ELF32_R_TYPE(i->r_info) == rel_type2) &&
                    (sym == &symtab->syms[ELF32_R_SYM(i->r_info)])) {
                    memset((char *)&section->getData()[i->r_offset - section->getAddr()], 0, entry_sz);
                    continue;
                }
            }
        }
        // Don't pack relocations happening in non writable sections.
        // Our injected code is likely not to be allowed to write there.
        if (!(section->getFlags() & SHF_WRITE) || (ELF32_R_TYPE(i->r_info) != rel_type) ||
            (relro && (i->r_offset >= relro->getAddr()) &&
                      (i->r_offset < relro->getAddr() + relro->getMemSize())))
            new_rels.push_back(*i);
        else {
            // TODO: check that i->r_addend == *i->r_offset
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
    // Last entry must be NULL
    relhack_entry.r_offset = relhack_entry.r_info = 0;
    relhack->push_back(relhack_entry);

    relhackcode->insertAfter(section);
    relhack->insertAfter(relhackcode);

    unsigned int old_end = section->getOffset() + section->getSize();
    section->rels.assign(new_rels.begin(), new_rels.end());
    section->shrink(new_rels.size() * section->getEntSize());
    ElfLocation *init = new ElfLocation(relhackcode, relhackcode->getEntryPoint());
    dyn->setValueForType(DT_INIT, init);
    // TODO: adjust the value according to the remaining number of relative relocations
    if (dyn->getValueForType(Rel_Type::d_tag_count))
        dyn->setValueForType(Rel_Type::d_tag_count, new ElfPlainValue(0));

    if (relhack->getOffset() + relhack->getSize() >= old_end) {
        fprintf(stderr, "No gain. Skipping\n");
        return -1;
    }
    return 0;
}

static inline int backup_file(const char *name)
{
    std::string fname(name);
    fname += ".bak";
    return rename(name, fname.c_str());
}

void do_file(const char *name, bool backup = false, bool force = false)
{
    std::ifstream file(name, std::ios::in|std::ios::binary);
    Elf *elf = new Elf(file);
    unsigned int size = elf->getSize();
    fprintf(stderr, "%s: ", name);
    if (elf->getType() != ET_DYN) {
        fprintf(stderr, "Not a shared object. Skipping\n");
        delete elf;
        return;
    }

    for (ElfSection *section = elf->getSection(1); section != NULL;
         section = section->getNext()) {
        if (section->getName() &&
            (strncmp(section->getName(), ".elfhack.", 9) == 0)) {
            fprintf(stderr, "Already elfhacked. Skipping\n");
            delete elf;
            return;
        }
    }

    int exit = -1;
    switch (elf->getMachine()) {
    case EM_386:
        exit = do_relocation_section<Elf_Rel>(elf, R_386_RELATIVE, R_386_32);
        break;
    case EM_X86_64:
        exit = do_relocation_section<Elf_Rela>(elf, R_X86_64_RELATIVE, R_X86_64_64);
        break;
    case EM_ARM:
        exit = do_relocation_section<Elf_Rel>(elf, R_ARM_RELATIVE, R_ARM_ABS32);
        break;
    }
    if (exit == 0) {
        if (!force && (elf->getSize() >= size)) {
            fprintf(stderr, "No gain. Skipping\n");
        } else if (backup && backup_file(name) != 0) {
            fprintf(stderr, "Couln't create backup file\n");
        } else {
            std::ofstream ofile(name, std::ios::out|std::ios::binary|std::ios::trunc);
            elf->write(ofile);
            fprintf(stderr, "Reduced by %d bytes\n", size - elf->getSize());
        }
    }
    delete elf;
}

int main(int argc, char *argv[])
{
    int arg;
    bool backup = false;
    bool force = false;
    char *lastSlash = rindex(argv[0], '/');
    if (lastSlash != NULL)
        rundir = strndup(argv[0], lastSlash - argv[0]);
    for (arg = 1; arg < argc; arg++) {
        if (strcmp(argv[arg], "-f") == 0)
            force = true;
        else if (strcmp(argv[arg], "-b") == 0)
            backup = true;
        else
            do_file(argv[arg], backup, force);
    }

    free(rundir);
    return 0;
}
