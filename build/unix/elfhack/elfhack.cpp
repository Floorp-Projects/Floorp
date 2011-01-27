/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is elfhack.
 *
 * The Initial Developer of the Original Code is
 * Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Mike Hommey <mh@glandium.org>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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

char *rundir = NULL;

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

        // Get all executable sections from the injected code object.
        // Most of the time, there will only be one for the init function,
        // but on e.g. x86, there is a separate section for
        // __i686.get_pc_thunk.$reg
        for (ElfSection *text = elf->getSection(1); text != NULL;
             text = text->getNext()) {
            if ((text->getType() == SHT_PROGBITS) &&
                (text->getFlags() & SHF_EXECINSTR)) {
                code.push_back(text);
                // We need to align this section depending on the greater
                // alignment required by code sections.
                if (shdr.sh_addralign < text->getAddrAlign())
                    shdr.sh_addralign = text->getAddrAlign();
            }
        }
        assert(code.size() != 0);

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

private:
    void apply_pc32_relocation(ElfSection *the_code, char *base, Elf_Rel *r, unsigned int addr)
    {
        *(Elf32_Addr *)(base + r->r_offset) += (Elf32_Addr) (addr - r->r_offset - the_code->getAddr());
    }

    void apply_pc32_relocation(ElfSection *the_code, char *base, Elf_Rela *r, unsigned int addr)
    {
        *(Elf32_Addr *)(base + r->r_offset) = (Elf32_Addr) (addr + r->r_addend - r->r_offset - the_code->getAddr());
    }

    void apply_arm_plt32_relocation(ElfSection *the_code, char *base, Elf_Rel *r, unsigned int addr)
    {
        // We don't care about sign_extend because the only case where this is
        // going to be used only jumps forward.
        Elf32_Addr addend = (Elf32_Addr) (addr - r->r_offset - the_code->getAddr()) >> 2;
        addend = (*(Elf32_Addr *)(base + r->r_offset) + addend) & 0x00ffffff;
        *(Elf32_Addr *)(base + r->r_offset) = (*(Elf32_Addr *)(base + r->r_offset) & 0xff000000) | addend;
    }

    void apply_arm_plt32_relocation(ElfSection *the_code, char *base, Elf_Rela *r, unsigned int addr)
    {
        // We don't care about sign_extend because the only case where this is
        // going to be used only jumps forward.
        Elf32_Addr addend = (Elf32_Addr) (addr - r->r_offset - the_code->getAddr()) >> 2;
        addend = (r->r_addend + addend) & 0x00ffffff;
        *(Elf32_Addr *)(base + r->r_offset) = (r->r_addend & 0xff000000) | addend;
    }

    void apply_gotoff_relocation(ElfSection *the_code, char *base, Elf_Rel *r, unsigned int addr)
    {
        *(Elf32_Addr *)(base + r->r_offset) += (Elf32_Addr) addr;
    }

    void apply_gotoff_relocation(ElfSection *the_code, char *base, Elf_Rela *r, unsigned int addr)
    {
        *(Elf32_Addr *)(base + r->r_offset) = (Elf32_Addr) addr + r->r_addend;
    }

    template <typename Rel_Type>
    void apply_relocations(ElfRel_Section<Rel_Type> *rel, ElfSection *the_code)
    {
        assert(rel->getType() == Rel_Type::sh_type);
        char *buf = data + (the_code->getAddr() - code.front()->getAddr());
        // TODO: various checks on the sections
        ElfSymtab_Section *symtab = (ElfSymtab_Section *)rel->getLink();
        ElfStrtab_Section *strtab = (ElfStrtab_Section *)symtab->getLink();
        for (typename std::vector<Rel_Type>::iterator r = rel->rels.begin(); r != rel->rels.end(); r++) {
            // TODO: various checks on the symbol
            const char *name = strtab->getStr(symtab->syms[ELF32_R_SYM(r->r_info)].st_name);
            unsigned int addr;
            if (symtab->syms[ELF32_R_SYM(r->r_info)].st_shndx == 0) {
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
                    fprintf(stderr, "Unsupported symbol in relocation: %s\n", name);
                    break;
                }
            } else {
                ElfSection *section = elf->getSection(symtab->syms[ELF32_R_SYM(r->r_info)].st_shndx);
                assert((section->getType() == SHT_PROGBITS) && (section->getFlags() & SHF_EXECINSTR));
                addr = section->getAddr() + symtab->syms[ELF32_R_SYM(r->r_info)].st_value;
            }
            // Do the relocation
#define REL(machine, type) (EM_ ## machine | (R_ ## machine ## _ ## type << 8))
            switch (elf->getMachine() | (ELF32_R_TYPE(r->r_info) << 8)) {
            case REL(X86_64, PC32):
            case REL(386, PC32):
            case REL(386, GOTPC):
            case REL(ARM, GOTPC):
                apply_pc32_relocation(the_code, buf, &*r, addr);
                break;
            case REL(ARM, PLT32):
                apply_arm_plt32_relocation(the_code, buf, &*r, addr);
                break;
            case REL(386, GOTOFF):
            case REL(ARM, GOTOFF):
                apply_gotoff_relocation(the_code, buf, &*r, addr);
                break;
            case REL(ARM, V4BX):
                // Ignore R_ARM_V4BX relocations
                break;
            default:
                fprintf(stderr, "Unsupported relocation type\n");
            }
        }
    }

    Elf *elf, &parent;
    std::vector<ElfSection *> code;
    ElfSection *init;
};

template <typename Rel_Type>
int do_relocation_section(Elf *elf, unsigned int rel_type)
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
        { 0, SHT_PROGBITS, SHF_ALLOC, 0, -1, 0, SHN_UNDEF, 0,
          Elf_RelHack::size(elf->getClass()), Elf_RelHack::size(elf->getClass()) }; // TODO: sh_addralign should be an alignment, not size
    Elf32_Shdr relhackcode32_section =
        { 0, SHT_PROGBITS, SHF_ALLOC | SHF_EXECINSTR, 0, -1, 0, SHN_UNDEF, 0, 1, 0 };
    Elf_Shdr relhack_section(relhack32_section);
    Elf_Shdr relhackcode_section(relhackcode32_section);
    ElfRelHack_Section *relhack = new ElfRelHack_Section(relhack_section);
    ElfRelHackCode_Section *relhackcode = new ElfRelHackCode_Section(relhackcode_section, *elf);
    relhackcode->insertAfter(section);
    relhack->insertAfter(relhackcode);

    std::vector<Rel_Type> new_rels;
    Elf_RelHack relhack_entry;
    relhack_entry.r_offset = relhack_entry.r_info = 0;
    int entry_sz = (elf->getClass() == ELFCLASS32) ? 4 : 8;
    for (typename std::vector<Rel_Type>::iterator i = section->rels.begin();
         i != section->rels.end(); i++) {
        // Don't pack relocations happening in non writable sections.
        // Our injected code is likely not to be allowed to write there.
        ElfSection *section = elf->getSectionAt(i->r_offset);
        if (!(section->getFlags() & SHF_WRITE) || (ELF32_R_TYPE(i->r_info) != rel_type) ||
            (relro && (i->r_offset >= relro->getFirstSection()->getAddr()) &&
                      (i->r_offset < relro->getFirstSection()->getAddr() + relro->getMemSize())))
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

    section->rels.assign(new_rels.begin(), new_rels.end());
    section->shrink(new_rels.size() * section->getEntSize());
    ElfLocation *init = new ElfLocation(relhackcode, 0);
    dyn->setValueForType(DT_INIT, init);
    // TODO: adjust the value according to the remaining number of relative relocations
    dyn->setValueForType(Rel_Type::d_tag_count, new ElfPlainValue(0));
    return 0;
}

static inline int backup_file(const char *name)
{
    std::string fname(name);
    fname += ".bak";
    return rename(name, fname.c_str());
}

void do_file(const char *name, bool backup = false)
{
    std::ifstream file(name, std::ios::in|std::ios::binary);
    Elf *elf = new Elf(file);
    unsigned int size = elf->getSize();
    fprintf(stderr, "%s: ", name);

    int exit = -1;
    switch (elf->getMachine()) {
    case EM_386:
        exit = do_relocation_section<Elf_Rel>(elf, R_386_RELATIVE);
        break;
    case EM_X86_64:
        exit = do_relocation_section<Elf_Rela>(elf, R_X86_64_RELATIVE);
        break;
    case EM_ARM:
        exit = do_relocation_section<Elf_Rel>(elf, R_ARM_RELATIVE);
        break;
    }
    if (elf->getSize() >= size)
        fprintf(stderr, "No gain. Aborting\n");
    else if (exit == 0) {
        if (backup && backup_file(name) != 0) {
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
    char *lastSlash = rindex(argv[0], '/');
    if (lastSlash != NULL)
        rundir = strndup(argv[0], lastSlash - argv[0]);
    for (arg = 1; arg < argc; arg++) {
        if (strcmp(argv[arg], "-b") == 0)
            backup = true;
        else
            do_file(argv[arg], backup);
    }

    free(rundir);
    return 0;
}
