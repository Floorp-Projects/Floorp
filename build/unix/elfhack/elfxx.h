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

#include <stdexcept>
#include <list>
#include <vector>
#include <cstring>
#include <iostream>
#include <fstream>
#include <algorithm>
#include <elf.h>
#include <asm/byteorder.h>

// Technically, __*_to_cpu and __cpu_to* function are equivalent,
// so swap can use either of both.
#define def_swap(endian, type, bits) \
static inline type ## bits ## _t swap(type ## bits ## _t i) { \
    return __ ## endian ## bits ## _to_cpu(i); \
}

class little_endian {
public:
def_swap(le, uint, 16);
def_swap(le, uint, 32);
def_swap(le, uint, 64);
def_swap(le, int, 16);
def_swap(le, int, 32);
def_swap(le, int, 64);
};

class big_endian {
public:
def_swap(be, uint, 16);
def_swap(be, uint, 32);
def_swap(be, uint, 64);
def_swap(be, int, 16);
def_swap(be, int, 32);
def_swap(be, int, 64);
};

// forward declaration
class ElfSection;
class ElfSegment;
// TODO: Rename Elf_* types
class Elf_Ehdr;
class Elf_Phdr;
class Elf;
class ElfDynamic_Section;
class ElfStrtab_Section;

class Elf_Ehdr_Traits {
public:
    typedef Elf32_Ehdr Type32;
    typedef Elf64_Ehdr Type64;

    template <class endian, typename R, typename T>
    static void swap(T &t, R &r);
};

class Elf_Phdr_Traits {
public:
    typedef Elf32_Phdr Type32;
    typedef Elf64_Phdr Type64;

    template <class endian, typename R, typename T>
    static void swap(T &t, R &r);
};

class Elf_Shdr_Traits {
public:
    typedef Elf32_Shdr Type32;
    typedef Elf64_Shdr Type64;

    template <class endian, typename R, typename T>
    static void swap(T &t, R &r);
};

class Elf_Dyn_Traits {
public:
    typedef Elf32_Dyn Type32;
    typedef Elf64_Dyn Type64;

    template <class endian, typename R, typename T>
    static void swap(T &t, R &r);
};

class Elf_Sym_Traits {
public:
    typedef Elf32_Sym Type32;
    typedef Elf64_Sym Type64;

    template <class endian, typename R, typename T>
    static void swap(T &t, R &r);
};

class Elf_Rel_Traits {
public:
    typedef Elf32_Rel Type32;
    typedef Elf64_Rel Type64;

    template <class endian, typename R, typename T>
    static void swap(T &t, R &r);
};

class Elf_Rela_Traits {
public:
    typedef Elf32_Rela Type32;
    typedef Elf64_Rela Type64;

    template <class endian, typename R, typename T>
    static void swap(T &t, R &r);
};

class ElfValue {
public:
    virtual unsigned int getValue() { return 0; }
    virtual ElfSection *getSection() { return NULL; }
};

class ElfPlainValue: public ElfValue {
    unsigned int value;
public:
    ElfPlainValue(unsigned int val): value(val) {};
    unsigned int getValue() { return value; }
};

class ElfLocation: public ElfValue {
    ElfSection *section;
    unsigned int offset;
public:
    enum position { ABSOLUTE, RELATIVE };
    ElfLocation(): section(NULL), offset(0) {};
    ElfLocation(ElfSection *section, unsigned int off, enum position pos = RELATIVE);
    ElfLocation(unsigned int location, Elf *elf);
    unsigned int getValue();
    ElfSection *getSection() { return section; }
};

class ElfSize: public ElfValue {
    ElfSection *section;
public:
    ElfSize(ElfSection *s): section(s) {};
    unsigned int getValue();
    ElfSection *getSection() { return section; }
};

class ElfEntSize: public ElfValue {
    ElfSection *section;
public:
    ElfEntSize(ElfSection *s): section(s) {};
    unsigned int getValue();
    ElfSection *getSection() { return section; }
};

template <typename T>
class serializable: public T::Type32 {
public:
    serializable() {};
    serializable(const typename T::Type32 &p): T::Type32(p) {};

private:
    template <typename R>
    void init(const char *buf, size_t len, char ei_data)
    {
        R e;
        assert(len <= sizeof(e));
        memcpy(&e, buf, sizeof(e));
        if (ei_data == ELFDATA2LSB) {
            T::template swap<little_endian>(e, *this);
            return;
        } else if (ei_data == ELFDATA2MSB) {
            T::template swap<big_endian>(e, *this);
            return;
        }
        throw std::runtime_error("Unsupported ELF data encoding");
    }

public:
    serializable(const char *buf, size_t len, char ei_class, char ei_data)
    {
        if (ei_class == ELFCLASS32) {
            init<typename T::Type32>(buf, len, ei_data);
            return;
        } else if (ei_class == ELFCLASS64) {
            init<typename T::Type64>(buf, len, ei_data);
            return;
        }
        throw std::runtime_error("Unsupported ELF class");
    }

    serializable(std::ifstream &file, char ei_class, char ei_data)
    {
        if (ei_class == ELFCLASS32) {
            typename T::Type32 e;
            file.read((char *)&e, sizeof(e));
            if (ei_data == ELFDATA2LSB) {
                T::template swap<little_endian>(e, *this);
                return;
            } else if (ei_data == ELFDATA2MSB) {
                T::template swap<big_endian>(e, *this);
                return;
            }
        } else if (ei_class == ELFCLASS64) {
            typename T::Type64 e;
            file.read((char *)&e, sizeof(e));
            if (ei_data == ELFDATA2LSB) {
                T::template swap<little_endian>(e, *this);
                return;
            } else if (ei_data == ELFDATA2MSB) {
                T::template swap<big_endian>(e, *this);
                return;
            }
        }
        throw std::runtime_error("Unsupported ELF class or data encoding");
    }

    void serialize(std::ofstream &file, char ei_class, char ei_data)
    {
        if (ei_class == ELFCLASS32) {
            typename T::Type32 e;
            if (ei_data == ELFDATA2LSB) {
                T::template swap<little_endian>(*this, e);
                file.write((char *)&e, sizeof(e));
                return;
            } else if (ei_data == ELFDATA2MSB) {
                T::template swap<big_endian>(*this, e);
                file.write((char *)&e, sizeof(e));
                return;
            }
        } else if (ei_class == ELFCLASS64) {
            typename T::Type64 e;
            if (ei_data == ELFDATA2LSB) {
                T::template swap<little_endian>(*this, e);
                file.write((char *)&e, sizeof(e));
                return;
            } else if (ei_data == ELFDATA2MSB) {
                T::template swap<big_endian>(*this, e);
                file.write((char *)&e, sizeof(e));
                return;
            }
        }
        throw std::runtime_error("Unsupported ELF class or data encoding");
    }

    static inline unsigned int size(char ei_class)
    {
        if (ei_class == ELFCLASS32)
            return sizeof(typename T::Type32);
        else if (ei_class == ELFCLASS64)
            return sizeof(typename T::Type64);
        return 0;
    }
};

typedef serializable<Elf_Shdr_Traits> Elf_Shdr;

class Elf {
public:
    Elf(std::ifstream &file);
    ~Elf();

    /* index == -1 is treated as index == ehdr.e_shstrndx */
    ElfSection *getSection(int index);

    ElfSection *getSectionAt(unsigned int offset);

    ElfSegment *getSegmentByType(unsigned int type);

    ElfDynamic_Section *getDynSection();

    void write(std::ofstream &file);

    char getClass();
    char getData();
    char getType();
    char getMachine();
    unsigned int getSize();
private:
    Elf_Ehdr *ehdr;
    ElfLocation eh_entry;
    ElfStrtab_Section *eh_shstrndx;
    ElfSection **sections;
    std::vector<ElfSegment *> segments;
    ElfSection *shdr_section, *phdr_section;
    /* Values used only during initialization */
    Elf_Shdr **tmp_shdr;
    std::ifstream *tmp_file;
};

class ElfSection {
public:
    typedef union {
        ElfSection *section;
        int index;
    } SectionInfo;

    ElfSection(Elf_Shdr &s, std::ifstream *file, Elf *parent);

    virtual ~ElfSection() {
        delete[] data;
    }

    const char *getName() { return name; }
    unsigned int getType() { return shdr.sh_type; }
    unsigned int getFlags() { return shdr.sh_flags; }
    unsigned int getAddr();
    unsigned int getSize() { return shdr.sh_size; }
    unsigned int getAddrAlign() { return shdr.sh_addralign; }
    unsigned int getEntSize() { return shdr.sh_entsize; }
    const char *getData() { return data; }
    ElfSection *getLink() { return link; }
    SectionInfo getInfo() { return info; }

    void shrink(unsigned int newsize) {
        if (newsize < shdr.sh_size) {
            shdr.sh_size = newsize;
            if (next)
                next->markDirty();
        }
    }

    unsigned int getOffset();
    int getIndex();
    Elf_Shdr &getShdr();

    ElfSection *getNext() { return next; }
    ElfSection *getPrevious() { return previous; }

    virtual bool isRelocatable() {
        return ((getType() == SHT_SYMTAB) ||
                (getType() == SHT_STRTAB) ||
                (getType() == SHT_RELA) ||
                (getType() == SHT_HASH) ||
                (getType() == SHT_NOTE) ||
                (getType() == SHT_REL) ||
                (getType() == SHT_DYNSYM) ||
                (getType() == SHT_GNU_HASH) ||
                (getType() == SHT_GNU_verdef) ||
                (getType() == SHT_GNU_verneed) ||
                (getType() == SHT_GNU_versym) ||
                isInSegmentType(PT_INTERP)) &&
                (getFlags() & SHF_ALLOC);
    }

    void insertAfter(ElfSection *section, bool dirty = true) {
        if (previous != NULL)
            previous->next = next;
        if (next != NULL)
            next->previous = previous;
        previous = section;
        if (section != NULL) {
            next = section->next;
            section->next = this;
        } else
            next = NULL;
        if (next != NULL)
            next->previous = this;
        if (dirty)
            markDirty();
    }

    void markDirty() {
        if (link != NULL)
            shdr.sh_link = -1;
        if (info.index)
            shdr.sh_info = -1;
        shdr.sh_offset = -1;
        if (isRelocatable())
            shdr.sh_addr = -1;
        if (next)
            next->markDirty();
    }

    virtual void serialize(std::ofstream &file, char ei_class, char ei_data)
    {
        if (getType() == SHT_NOBITS)
            return;
        file.seekp(getOffset());
        file.write(data, getSize());
    }

private:
    friend class ElfSegment;

    void addToSegment(ElfSegment *segment) {
        segments.push_back(segment);
    }

    void removeFromSegment(ElfSegment *segment) {
        std::vector<ElfSegment *>::iterator i = std::find(segments.begin(), segments.end(), segment);
        segments.erase(i, i + 1);
    }

    bool isInSegmentType(unsigned int type);
protected:
    Elf_Shdr shdr;
    char *data;
    const char *name;
private:
    ElfSection *link;
    SectionInfo info;
    ElfSection *next, *previous;
    int index;
    std::vector<ElfSegment *> segments;
};

class ElfSegment {
public:
    ElfSegment(Elf_Phdr *phdr);

    unsigned int getType() { return type; }
    unsigned int getFlags() { return flags; }
    unsigned int getAlign() { return type == PT_LOAD ? 0x1000 : align; /* TODO: remove this gross hack */ }

    ElfSection *getFirstSection() { return sections.empty() ? NULL : sections.front(); }
    int getVPDiff() { return v_p_diff; }
    unsigned int getFileSize();
    unsigned int getMemSize();
    unsigned int getOffset();
    unsigned int getAddr();

    void addSection(ElfSection *section);

    std::list<ElfSection *>::iterator begin() { return sections.begin(); }
    std::list<ElfSection *>::iterator end() { return sections.end(); }

    ElfSegment *splitBefore(ElfSection *section);
private:
    unsigned int type;
    int v_p_diff; // Difference between physical and virtual address
    unsigned int flags;
    unsigned int align;
    std::list<ElfSection *> sections;
    // The following are only really used for PT_GNU_RELRO until something
    // better is found.
    unsigned int vaddr;
    unsigned int filesz, memsz;
};

class Elf_Ehdr: public serializable<Elf_Ehdr_Traits>, public ElfSection {
public:
    Elf_Ehdr(std::ifstream &file, char ei_class, char ei_data);
    void serialize(std::ofstream &file, char ei_class, char ei_data)
    {
        serializable<Elf_Ehdr_Traits>::serialize(file, ei_class, ei_data);
    }
};

class Elf_Phdr: public serializable<Elf_Phdr_Traits> {
public:
    Elf_Phdr() {};
    Elf_Phdr(std::ifstream &file, char ei_class, char ei_data)
    : serializable<Elf_Phdr_Traits>(file, ei_class, ei_data) {};
    bool contains(ElfSection *section)
    {
        unsigned int size = section->getSize();
        unsigned int addr = section->getAddr();
        // This may be biased, but should work in most cases
        if ((section->getFlags() & SHF_ALLOC) == 0)
            return false;
        // Special case for PT_DYNAMIC. Eventually, this should
        // be better handled than special cases
        if ((p_type == PT_DYNAMIC) && (section->getType() != SHT_DYNAMIC))
            return false;
        // Special case for PT_TLS.
        if ((p_type == PT_TLS) && !(section->getFlags() & SHF_TLS))
            return false;
        return (addr >= p_vaddr) &&
               (addr + size <= p_vaddr + p_memsz);

    }
};

typedef serializable<Elf_Dyn_Traits> Elf_Dyn;

struct Elf_DynValue {
    unsigned int tag;
    ElfValue *value;
};

class ElfDynamic_Section: public ElfSection {
public:
    ElfDynamic_Section(Elf_Shdr &s, std::ifstream *file, Elf *parent);
    ~ElfDynamic_Section();

    void serialize(std::ofstream &file, char ei_class, char ei_data);

    ElfValue *getValueForType(unsigned int tag);
    ElfSection *getSectionForType(unsigned int tag);
    void setValueForType(unsigned int tag, ElfValue *val);
private:
    std::vector<Elf_DynValue> dyns;
};

typedef serializable<Elf_Sym_Traits> Elf_Sym;

struct Elf_SymValue {
    const char *name;
    unsigned char info;
    unsigned char other;
    ElfLocation value;
    unsigned int size;
    bool defined;
};

#define STT(type) (1 << STT_ ##type)

class ElfSymtab_Section: public ElfSection {
public:
    ElfSymtab_Section(Elf_Shdr &s, std::ifstream *file, Elf *parent);

    void serialize(std::ofstream &file, char ei_class, char ei_data);

    Elf_SymValue *lookup(const char *name, unsigned int type_filter = STT(OBJECT) | STT(FUNC));

//private: // Until we have a real API
    std::vector<Elf_SymValue> syms;
};

class Elf_Rel: public serializable<Elf_Rel_Traits> {
public:
    Elf_Rel(std::ifstream &file, char ei_class, char ei_data)
    : serializable<Elf_Rel_Traits>(file, ei_class, ei_data) {};

    static const unsigned int sh_type = SHT_REL;
    static const unsigned int d_tag = DT_REL;
    static const unsigned int d_tag_count = DT_RELCOUNT;
};

class Elf_Rela: public serializable<Elf_Rela_Traits> {
public:
    Elf_Rela(std::ifstream &file, char ei_class, char ei_data)
    : serializable<Elf_Rela_Traits>(file, ei_class, ei_data) {};

    static const unsigned int sh_type = SHT_RELA;
    static const unsigned int d_tag = DT_RELA;
    static const unsigned int d_tag_count = DT_RELACOUNT;
};

template <class Rel>
class ElfRel_Section: public ElfSection {
public:
    ElfRel_Section(Elf_Shdr &s, std::ifstream *file, Elf *parent)
    : ElfSection(s, file, parent)
    {
        int pos = file->tellg();
        file->seekg(shdr.sh_offset);
        for (unsigned int i = 0; i < s.sh_size / s.sh_entsize; i++) {
            Rel r(*file, parent->getClass(), parent->getData());
            rels.push_back(r);
        }
        file->seekg(pos);
    }

    void serialize(std::ofstream &file, char ei_class, char ei_data)
    {
        for (typename std::vector<Rel>::iterator i = rels.begin();
             i != rels.end(); ++i)
            (*i).serialize(file, ei_class, ei_data);
    }
//private: // Until we have a real API
    std::vector<Rel> rels;
};

class ElfStrtab_Section: public ElfSection {
public:
    ElfStrtab_Section(Elf_Shdr &s, std::ifstream *file, Elf *parent)
    : ElfSection(s, file, parent)
    {
        table.push_back(table_storage(data, shdr.sh_size));
    }

    ~ElfStrtab_Section()
    {
        for (std::vector<table_storage>::iterator t = table.begin() + 1;
             t != table.end(); t++)
            delete[] t->buf;
    }

    const char *getStr(unsigned int index);

    const char *getStr(const char *string);

    unsigned int getStrIndex(const char *string);

    void serialize(std::ofstream &file, char ei_class, char ei_data);
private:
    struct table_storage {
        unsigned int size, used;
        char *buf;

        table_storage(): size(4096), used(0), buf(new char[4096]) {}
        table_storage(const char *data, unsigned int sz)
        : size(sz), used(sz), buf(const_cast<char *>(data)) {}
    };
    std::vector<table_storage> table;
};

inline char Elf::getClass() {
    return ehdr->e_ident[EI_CLASS];
}

inline char Elf::getData() {
    return ehdr->e_ident[EI_DATA];
}

inline char Elf::getType() {
    return ehdr->e_type;
}

inline char Elf::getMachine() {
    return ehdr->e_machine;
}

inline unsigned int Elf::getSize() {
    ElfSection *section;
    for (section = shdr_section /* It's usually not far from the end */;
        section->getNext() != NULL; section = section->getNext());
    return section->getOffset() + section->getSize();
}

inline bool ElfSection::isInSegmentType(unsigned int type) {
    for (std::vector<ElfSegment *>::iterator seg = segments.begin(); seg != segments.end(); seg++)
        if ((*seg)->getType() == type)
            return true;
    return false;
}

inline ElfLocation::ElfLocation(ElfSection *section, unsigned int off, enum position pos)
: section(section) {
    if ((pos == ABSOLUTE) && section)
        offset = off - section->getAddr();
    else
        offset = off;
}

inline ElfLocation::ElfLocation(unsigned int location, Elf *elf) {
    section = elf->getSectionAt(location);
    offset = location - (section ? section->getAddr() : 0);
}

inline unsigned int ElfLocation::getValue() {
    return (section ? section->getAddr() : 0) + offset;
}

inline unsigned int ElfSize::getValue() {
    return section->getSize();
}

inline unsigned int ElfEntSize::getValue() {
    return section->getEntSize();
}
