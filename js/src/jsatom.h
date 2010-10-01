/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#ifndef jsatom_h___
#define jsatom_h___
/*
 * JS atom table.
 */
#include <stddef.h>
#include "jsversion.h"
#include "jsapi.h"
#include "jsprvtd.h"
#include "jshash.h"
#include "jshashtable.h"
#include "jspubtd.h"
#include "jsstr.h"
#include "jslock.h"
#include "jsvalue.h"

#define ATOM_PINNED     0x1       /* atom is pinned against GC */
#define ATOM_INTERNED   0x2       /* pinned variant for JS_Intern* API */
#define ATOM_NOCOPY     0x4       /* don't copy atom string bytes */
#define ATOM_TMPSTR     0x8       /* internal, to avoid extra string */

#define STRING_TO_ATOM(str)       (JS_ASSERT(str->isAtomized()),             \
                                   (JSAtom *)str)
#define ATOM_TO_STRING(atom)      ((JSString *)(atom))
#define ATOM_TO_JSVAL(atom)       STRING_TO_JSVAL(ATOM_TO_STRING(atom))

/* Engine-internal extensions of jsid */

static JS_ALWAYS_INLINE jsid
JSID_FROM_BITS(size_t bits)
{
    jsid id;
    JSID_BITS(id) = bits;
    return id;
}

static JS_ALWAYS_INLINE jsid
ATOM_TO_JSID(JSAtom *atom)
{
    JS_ASSERT(((size_t)atom & 0x7) == 0);
    return JSID_FROM_BITS((size_t)atom);
}

/* All strings stored in jsids are atomized. */
static JS_ALWAYS_INLINE JSBool
JSID_IS_ATOM(jsid id)
{
    return JSID_IS_STRING(id);
}

static JS_ALWAYS_INLINE JSBool
JSID_IS_ATOM(jsid id, JSAtom *atom)
{
    return JSID_BITS(id) == JSID_BITS(ATOM_TO_JSID(atom));
}

static JS_ALWAYS_INLINE JSAtom *
JSID_TO_ATOM(jsid id)
{
    return (JSAtom *)JSID_TO_STRING(id);
}

namespace js {

static JS_ALWAYS_INLINE Value
IdToValue(jsid id)
{
    if (JSID_IS_STRING(id))
        return StringValue(JSID_TO_STRING(id));
    if (JS_LIKELY(JSID_IS_INT(id)))
        return Int32Value(JSID_TO_INT(id));
    if (JS_LIKELY(JSID_IS_OBJECT(id)))
        return ObjectValue(*JSID_TO_OBJECT(id));
    JS_ASSERT(JSID_IS_DEFAULT_XML_NAMESPACE(id) || JSID_IS_VOID(id));
    return UndefinedValue();
}

static JS_ALWAYS_INLINE jsval
IdToJsval(jsid id)
{
    return Jsvalify(IdToValue(id));
}

}

#if JS_BYTES_PER_WORD == 4
# define ATOM_HASH(atom)          ((JSHashNumber)(atom) >> 2)
#elif JS_BYTES_PER_WORD == 8
# define ATOM_HASH(atom)          (((JSHashNumber)(jsuword)(atom) >> 3) ^     \
                                   (JSHashNumber)((jsuword)(atom) >> 32))
#else
# error "Unsupported configuration"
#endif

/*
 * Return a printable, lossless char[] representation of a string-type atom.
 * The lifetime of the result extends at least until the next GC activation,
 * longer if cx's string newborn root is not overwritten.
 */
extern const char *
js_AtomToPrintableString(JSContext *cx, JSAtom *atom);

struct JSAtomListElement {
    JSHashEntry         entry;
};

#define ALE_ATOM(ale)   ((JSAtom *) (ale)->entry.key)
#define ALE_INDEX(ale)  (jsatomid(uintptr_t((ale)->entry.value)))
#define ALE_VALUE(ale)  ((jsboxedword) (ale)->entry.value)
#define ALE_NEXT(ale)   ((JSAtomListElement *) (ale)->entry.next)

/*
 * In an upvars list, ALE_DEFN(ale)->resolve() is the outermost definition the
 * name may reference. If a with block or a function that calls eval encloses
 * the use, the name may end up referring to something else at runtime.
 */
#define ALE_DEFN(ale)   ((JSDefinition *) (ale)->entry.value)

#define ALE_SET_ATOM(ale,atom)  ((ale)->entry.key = (const void *)(atom))
#define ALE_SET_INDEX(ale,index)((ale)->entry.value = (void *)(index))
#define ALE_SET_DEFN(ale, dn)   ((ale)->entry.value = (void *)(dn))
#define ALE_SET_VALUE(ale, v)   ((ale)->entry.value = (void *)(v))
#define ALE_SET_NEXT(ale,nxt)   ((ale)->entry.next = (JSHashEntry *)(nxt))

/*
 * NB: JSAtomSet must be plain-old-data as it is embedded in the pn_u union in
 * JSParseNode. JSAtomList encapsulates all operational uses of a JSAtomSet.
 *
 * The JSAtomList name is traditional, even though the implementation is a map
 * (not to be confused with JSAtomMap). In particular the "ALE" and "ale" short
 * names for JSAtomListElement variables roll off the fingers, compared to ASE
 * or AME alternatives.
 */
struct JSAtomSet {
    JSHashEntry         *list;          /* literals indexed for mapping */
    JSHashTable         *table;         /* hash table if list gets too long */
    jsuint              count;          /* count of indexed literals */
};

struct JSAtomList : public JSAtomSet
{
#ifdef DEBUG
    const JSAtomSet* set;               /* asserted null in mutating methods */
#endif

    JSAtomList() {
        list = NULL; table = NULL; count = 0;
#ifdef DEBUG
        set = NULL;
#endif
    }

    JSAtomList(const JSAtomSet& as) {
        list = as.list; table = as.table; count = as.count;
#ifdef DEBUG
        set = &as;
#endif
    }

    void clear() { JS_ASSERT(!set); list = NULL; table = NULL; count = 0; }

    JSAtomListElement *lookup(JSAtom *atom) {
        JSHashEntry **hep;
        return rawLookup(atom, hep);
    }

    JSAtomListElement *rawLookup(JSAtom *atom, JSHashEntry **&hep);

    enum AddHow { UNIQUE, SHADOW, HOIST };

    JSAtomListElement *add(js::Parser *parser, JSAtom *atom, AddHow how = UNIQUE);

    void remove(js::Parser *parser, JSAtom *atom) {
        JSHashEntry **hep;
        JSAtomListElement *ale = rawLookup(atom, hep);
        if (ale)
            rawRemove(parser, ale, hep);
    }

    void rawRemove(js::Parser *parser, JSAtomListElement *ale, JSHashEntry **hep);
};

/*
 * A subclass of JSAtomList with a destructor.  This atom list owns its
 * hash table and its entries, but no keys or values.
 */
struct JSAutoAtomList: public JSAtomList
{
    JSAutoAtomList(js::Parser *p): parser(p) {}
    ~JSAutoAtomList();
  private:
    js::Parser *parser;         /* For freeing list entries. */
};

/*
 * Iterate over an atom list. We define a call operator to minimize the syntax
 * tax for users. We do not use a more standard pattern using ++ and * because
 * (a) it's the wrong pattern for a non-scalar; (b) it's overkill -- one method
 * is enough. (This comment is overkill!)
 */
class JSAtomListIterator {
    JSAtomList*         list;
    JSAtomListElement*  next;
    uint32              index;

  public:
    JSAtomListIterator(JSAtomList* al) : list(al) { reset(); }

    void reset() {
        next = (JSAtomListElement *) list->list;
        index = 0;
    }

    JSAtomListElement* operator ()();
};

struct JSAtomMap {
    JSAtom              **vector;       /* array of ptrs to indexed atoms */
    jsatomid            length;         /* count of (to-be-)indexed atoms */
};

namespace js {

#define ATOM_ENTRY_FLAG_MASK            ((size_t)(ATOM_PINNED | ATOM_INTERNED))

JS_STATIC_ASSERT(ATOM_ENTRY_FLAG_MASK < JS_GCTHING_ALIGN);

typedef uintptr_t AtomEntryType;

static JS_ALWAYS_INLINE JSString *
AtomEntryToKey(AtomEntryType entry)
{
    JS_ASSERT(entry != 0);
    return (JSString *)(entry & ~ATOM_ENTRY_FLAG_MASK);
}

struct AtomHasher
{
    typedef JSString *Lookup;

    static HashNumber hash(JSString *str) {
        return js_HashString(str);
    }

    static bool match(AtomEntryType entry, JSString *lookup) {
        return entry ? js_EqualStrings(AtomEntryToKey(entry), lookup) : false;
    }
};

typedef HashSet<AtomEntryType, AtomHasher, SystemAllocPolicy> AtomSet;

}  /* namespace js */

struct JSAtomState
{
    js::AtomSet         atoms;

#ifdef JS_THREADSAFE
    JSThinLock          lock;
#endif

    /*
     * From this point until the end of struct definition the struct must
     * contain only JSAtom fields. We use this to access the storage occupied
     * by the common atoms in js_FinishCommonAtoms.
     *
     * js_common_atom_names defined in jsatom.c contains C strings for atoms
     * in the order of atom fields here. Therefore you must update that array
     * if you change member order here.
     */

    /* The rt->emptyString atom, see jsstr.c's js_InitRuntimeStringState. */
    JSAtom              *emptyAtom;

    /*
     * Literal value and type names.
     * NB: booleanAtoms must come right before typeAtoms!
     */
    JSAtom              *booleanAtoms[2];
    JSAtom              *typeAtoms[JSTYPE_LIMIT];
    JSAtom              *nullAtom;

    /* Standard class constructor or prototype names. */
    JSAtom              *classAtoms[JSProto_LIMIT];

    /* Various built-in or commonly-used atoms, pinned on first context. */
    JSAtom              *anonymousAtom;
    JSAtom              *applyAtom;
    JSAtom              *argumentsAtom;
    JSAtom              *arityAtom;
    JSAtom              *callAtom;
    JSAtom              *calleeAtom;
    JSAtom              *callerAtom;
    JSAtom              *classPrototypeAtom;
    JSAtom              *constructorAtom;
    JSAtom              *eachAtom;
    JSAtom              *evalAtom;
    JSAtom              *fileNameAtom;
    JSAtom              *getAtom;
    JSAtom              *globalAtom;
    JSAtom              *ignoreCaseAtom;
    JSAtom              *indexAtom;
    JSAtom              *inputAtom;
    JSAtom              *toISOStringAtom;
    JSAtom              *iteratorAtom;
    JSAtom              *joinAtom;
    JSAtom              *lastIndexAtom;
    JSAtom              *lengthAtom;
    JSAtom              *lineNumberAtom;
    JSAtom              *messageAtom;
    JSAtom              *multilineAtom;
    JSAtom              *nameAtom;
    JSAtom              *nextAtom;
    JSAtom              *noSuchMethodAtom;
    JSAtom              *protoAtom;
    JSAtom              *setAtom;
    JSAtom              *sourceAtom;
    JSAtom              *stackAtom;
    JSAtom              *stickyAtom;
    JSAtom              *toGMTStringAtom;
    JSAtom              *toLocaleStringAtom;
    JSAtom              *toSourceAtom;
    JSAtom              *toStringAtom;
    JSAtom              *toUTCStringAtom;
    JSAtom              *valueOfAtom;
    JSAtom              *toJSONAtom;
    JSAtom              *void0Atom;
    JSAtom              *enumerableAtom;
    JSAtom              *configurableAtom;
    JSAtom              *writableAtom;
    JSAtom              *valueAtom;
    JSAtom              *testAtom;
    JSAtom              *useStrictAtom;

#if JS_HAS_XML_SUPPORT
    JSAtom              *etagoAtom;
    JSAtom              *namespaceAtom;
    JSAtom              *ptagcAtom;
    JSAtom              *qualifierAtom;
    JSAtom              *spaceAtom;
    JSAtom              *stagoAtom;
    JSAtom              *starAtom;
    JSAtom              *starQualifierAtom;
    JSAtom              *tagcAtom;
    JSAtom              *xmlAtom;

    /* Represents an invalid URI, for internal use only. */
    JSAtom              *functionNamespaceURIAtom;
#endif

    JSAtom              *ProxyAtom;

    JSAtom              *getOwnPropertyDescriptorAtom;
    JSAtom              *getPropertyDescriptorAtom;
    JSAtom              *definePropertyAtom;
    JSAtom              *deleteAtom;
    JSAtom              *getOwnPropertyNamesAtom;
    JSAtom              *enumerateAtom;
    JSAtom              *fixAtom;

    JSAtom              *hasAtom;
    JSAtom              *hasOwnAtom;
    JSAtom              *enumerateOwnAtom;
    JSAtom              *iterateAtom;

    /* Less frequently used atoms, pinned lazily by JS_ResolveStandardClass. */
    struct {
        JSAtom          *InfinityAtom;
        JSAtom          *NaNAtom;
        JSAtom          *XMLListAtom;
        JSAtom          *decodeURIAtom;
        JSAtom          *decodeURIComponentAtom;
        JSAtom          *defineGetterAtom;
        JSAtom          *defineSetterAtom;
        JSAtom          *encodeURIAtom;
        JSAtom          *encodeURIComponentAtom;
        JSAtom          *escapeAtom;
        JSAtom          *hasOwnPropertyAtom;
        JSAtom          *isFiniteAtom;
        JSAtom          *isNaNAtom;
        JSAtom          *isPrototypeOfAtom;
        JSAtom          *isXMLNameAtom;
        JSAtom          *lookupGetterAtom;
        JSAtom          *lookupSetterAtom;
        JSAtom          *parseFloatAtom;
        JSAtom          *parseIntAtom;
        JSAtom          *propertyIsEnumerableAtom;
        JSAtom          *unescapeAtom;
        JSAtom          *unevalAtom;
        JSAtom          *unwatchAtom;
        JSAtom          *watchAtom;
    } lazy;
};

#define ATOM(name) cx->runtime->atomState.name##Atom

#define ATOM_OFFSET_START       offsetof(JSAtomState, emptyAtom)
#define LAZY_ATOM_OFFSET_START  offsetof(JSAtomState, lazy)
#define ATOM_OFFSET_LIMIT       (sizeof(JSAtomState))

#define COMMON_ATOMS_START(state)                                             \
    ((JSAtom **)((uint8 *)(state) + ATOM_OFFSET_START))
#define COMMON_ATOM_INDEX(name)                                               \
    ((offsetof(JSAtomState, name##Atom) - ATOM_OFFSET_START)                  \
     / sizeof(JSAtom*))
#define COMMON_TYPE_ATOM_INDEX(type)                                          \
    ((offsetof(JSAtomState, typeAtoms[type]) - ATOM_OFFSET_START)             \
     / sizeof(JSAtom*))

#define ATOM_OFFSET(name)       offsetof(JSAtomState, name##Atom)
#define OFFSET_TO_ATOM(rt,off)  (*(JSAtom **)((char*)&(rt)->atomState + (off)))
#define CLASS_ATOM_OFFSET(name) offsetof(JSAtomState,classAtoms[JSProto_##name])

#define CLASS_ATOM(cx,name) \
    ((cx)->runtime->atomState.classAtoms[JSProto_##name])

extern const char *const js_common_atom_names[];
extern const size_t      js_common_atom_count;

/*
 * Macros to access C strings for JSType and boolean literals.
 */
#define JS_BOOLEAN_STR(type) (js_common_atom_names[1 + (type)])
#define JS_TYPE_STR(type)    (js_common_atom_names[1 + 2 + (type)])

/* Well-known predefined C strings. */
#define JS_PROTO(name,code,init) extern const char js_##name##_str[];
#include "jsproto.tbl"
#undef JS_PROTO

extern const char   js_anonymous_str[];
extern const char   js_apply_str[];
extern const char   js_arguments_str[];
extern const char   js_arity_str[];
extern const char   js_call_str[];
extern const char   js_callee_str[];
extern const char   js_caller_str[];
extern const char   js_class_prototype_str[];
extern const char   js_close_str[];
extern const char   js_constructor_str[];
extern const char   js_count_str[];
extern const char   js_etago_str[];
extern const char   js_each_str[];
extern const char   js_eval_str[];
extern const char   js_fileName_str[];
extern const char   js_get_str[];
extern const char   js_getter_str[];
extern const char   js_global_str[];
extern const char   js_ignoreCase_str[];
extern const char   js_index_str[];
extern const char   js_input_str[];
extern const char   js_iterator_str[];
extern const char   js_join_str[];
extern const char   js_lastIndex_str[];
extern const char   js_length_str[];
extern const char   js_lineNumber_str[];
extern const char   js_message_str[];
extern const char   js_multiline_str[];
extern const char   js_name_str[];
extern const char   js_namespace_str[];
extern const char   js_next_str[];
extern const char   js_noSuchMethod_str[];
extern const char   js_object_str[];
extern const char   js_proto_str[];
extern const char   js_ptagc_str[];
extern const char   js_qualifier_str[];
extern const char   js_send_str[];
extern const char   js_setter_str[];
extern const char   js_set_str[];
extern const char   js_source_str[];
extern const char   js_space_str[];
extern const char   js_stack_str[];
extern const char   js_sticky_str[];
extern const char   js_stago_str[];
extern const char   js_star_str[];
extern const char   js_starQualifier_str[];
extern const char   js_tagc_str[];
extern const char   js_toGMTString_str[];
extern const char   js_toLocaleString_str[];
extern const char   js_toSource_str[];
extern const char   js_toString_str[];
extern const char   js_toUTCString_str[];
extern const char   js_undefined_str[];
extern const char   js_valueOf_str[];
extern const char   js_toJSON_str[];
extern const char   js_xml_str[];
extern const char   js_enumerable_str[];
extern const char   js_configurable_str[];
extern const char   js_writable_str[];
extern const char   js_value_str[];
extern const char   js_test_str[];

/*
 * Initialize atom state. Return true on success, false on failure to allocate
 * memory. The caller must zero rt->atomState before calling this function and
 * only call it after js_InitGC successfully returns.
 */
extern JSBool
js_InitAtomState(JSRuntime *rt);

/*
 * Free and clear atom state including any interned string atoms. This
 * function must be called before js_FinishGC.
 */
extern void
js_FinishAtomState(JSRuntime *rt);

/*
 * Atom tracing and garbage collection hooks.
 */

extern void
js_TraceAtomState(JSTracer *trc);

extern void
js_SweepAtomState(JSContext *cx);

extern JSBool
js_InitCommonAtoms(JSContext *cx);

extern void
js_FinishCommonAtoms(JSContext *cx);

/*
 * Find or create the atom for a string. Return null on failure to allocate
 * memory.
 */
extern JSAtom *
js_AtomizeString(JSContext *cx, JSString *str, uintN flags);

extern JSAtom *
js_Atomize(JSContext *cx, const char *bytes, size_t length, uintN flags);

extern JSAtom *
js_AtomizeChars(JSContext *cx, const jschar *chars, size_t length, uintN flags);

/*
 * Return an existing atom for the given char array or null if the char
 * sequence is currently not atomized.
 */
extern JSAtom *
js_GetExistingStringAtom(JSContext *cx, const jschar *chars, size_t length);

#ifdef DEBUG

extern JS_FRIEND_API(void)
js_DumpAtoms(JSContext *cx, FILE *fp);

#endif

inline bool
js_ValueToAtom(JSContext *cx, const js::Value &v, JSAtom **atomp);

inline bool
js_ValueToStringId(JSContext *cx, const js::Value &v, jsid *idp);

inline bool
js_InternNonIntElementId(JSContext *cx, JSObject *obj, const js::Value &idval,
                         jsid *idp);
inline bool
js_InternNonIntElementId(JSContext *cx, JSObject *obj, const js::Value &idval,
                         jsid *idp, js::Value *vp);
/*
 * For all unmapped atoms recorded in al, add a mapping from the atom's index
 * to its address. map->length must already be set to the number of atoms in
 * the list and map->vector must point to pre-allocated memory.
 */
extern void
js_InitAtomMap(JSContext *cx, JSAtomMap *map, JSAtomList *al);

#endif /* jsatom_h___ */
