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

#include <stddef.h>
#include "jsversion.h"
#include "jsapi.h"
#include "jsprvtd.h"
#include "jshashtable.h"
#include "jspubtd.h"
#include "jsstr.h"
#include "jslock.h"
#include "jsvalue.h"

#include "vm/String.h"

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

extern jsid
js_CheckForStringIndex(jsid id);

JS_STATIC_ASSERT(sizeof(JSHashNumber) == 4);
JS_STATIC_ASSERT(sizeof(jsid) == JS_BYTES_PER_WORD);

namespace js {

static JS_ALWAYS_INLINE JSHashNumber
HashId(jsid id)
{
    JS_ASSERT(js_CheckForStringIndex(id) == id);
    JSHashNumber n =
#if JS_BYTES_PER_WORD == 4
        JSHashNumber(JSID_BITS(id));
#elif JS_BYTES_PER_WORD == 8
        JSHashNumber(JSID_BITS(id)) ^ JSHashNumber(JSID_BITS(id) >> 32);
#else
# error "Unsupported configuration"
#endif
    return n * JS_GOLDEN_RATIO;
}

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

template<>
struct DefaultHasher<jsid>
{
    typedef jsid Lookup;
    static HashNumber hash(const Lookup &l) {
        JS_ASSERT(l == js_CheckForStringIndex(l));
        return HashNumber(JSID_BITS(l));
    }
    static bool match(const jsid &id, const Lookup &l) {
        JS_ASSERT(l == js_CheckForStringIndex(l));
        return id == l;
    }
};

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
 * The lifetime of the result matches the lifetime of bytes.
 */
extern const char *
js_AtomToPrintableString(JSContext *cx, JSAtom *atom, JSAutoByteString *bytes);

struct JSAtomMap {
    JSAtom **vector;    /* array of ptrs to indexed atoms */
    uint32 length;      /* count of (to-be-)indexed atoms */
};

namespace js {

/* N.B. must correspond to boolean tagging behavior. */
enum InternBehavior
{
    DoNotInternAtom = false,
    InternAtom = true
};

typedef TaggedPointerEntry<JSAtom> AtomStateEntry;

struct AtomHasher
{
    struct Lookup
    {
        const jschar    *chars;
        size_t          length;
        const JSAtom    *atom; /* Optional. */

        Lookup(const jschar *chars, size_t length) : chars(chars), length(length), atom(NULL) {}
        Lookup(const JSAtom *atom) : chars(atom->chars()), length(atom->length()), atom(atom) {}
    };

    static HashNumber hash(const Lookup &l) {
        return HashChars(l.chars, l.length);
    }

    static bool match(const AtomStateEntry &entry, const Lookup &lookup) {
        JSAtom *key = entry.asPtr();

        if (lookup.atom)
            return lookup.atom == key;
        if (key->length() != lookup.length)
            return false;
        return PodEqual(key->chars(), lookup.chars, lookup.length);
    }
};

typedef HashSet<AtomStateEntry, AtomHasher, SystemAllocPolicy> AtomSet;

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
    JSAtom              *BYTES_PER_ELEMENTAtom;
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
    JSAtom              *objectNullAtom;
    JSAtom              *objectUndefinedAtom;
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
    JSAtom              *locAtom;
    JSAtom              *lineAtom;
    JSAtom              *InfinityAtom;
    JSAtom              *NaNAtom;
    JSAtom              *builderAtom;

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
    JSAtom              *keysAtom;
    JSAtom              *iterateAtom;

    JSAtom              *WeakMapAtom;

    JSAtom              *byteLengthAtom;

    /* Less frequently used atoms, pinned lazily by JS_ResolveStandardClass. */
    struct {
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

    static const size_t commonAtomsOffset;
    static const size_t lazyAtomsOffset;

    void clearLazyAtoms() {
        memset(&lazy, 0, sizeof(lazy));
    }

    void junkAtoms() {
#ifdef DEBUG
        memset(commonAtomsStart(), JS_FREE_PATTERN, sizeof(*this) - commonAtomsOffset);
#endif
    }

    JSAtom **commonAtomsStart() {
        return &emptyAtom;
    }

    void checkStaticInvariants();
};

extern bool
AtomIsInterned(JSContext *cx, JSAtom *atom);

#define ATOM(name) cx->runtime->atomState.name##Atom

#define COMMON_ATOM_INDEX(name)                                               \
    ((offsetof(JSAtomState, name##Atom) - JSAtomState::commonAtomsOffset)     \
     / sizeof(JSAtom*))
#define COMMON_TYPE_ATOM_INDEX(type)                                          \
    ((offsetof(JSAtomState, typeAtoms[type]) - JSAtomState::commonAtomsOffset)\
     / sizeof(JSAtom*))

#define ATOM_OFFSET(name)       offsetof(JSAtomState, name##Atom)
#define OFFSET_TO_ATOM(rt,off)  (*(JSAtom **)((char*)&(rt)->atomState + (off)))
#define CLASS_ATOM_OFFSET(name) offsetof(JSAtomState, classAtoms[JSProto_##name])
#define CLASS_ATOM(cx,name)     ((cx)->runtime->atomState.classAtoms[JSProto_##name])

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
extern const char   js_BYTES_PER_ELEMENT_str[];
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

extern bool
js_InitCommonAtoms(JSContext *cx);

extern void
js_FinishCommonAtoms(JSContext *cx);

/*
 * Find or create the atom for a string. Return null on failure to allocate
 * memory.
 */
extern JSAtom *
js_AtomizeString(JSContext *cx, JSString *str, js::InternBehavior ib = js::DoNotInternAtom);

extern JSAtom *
js_Atomize(JSContext *cx, const char *bytes, size_t length,
           js::InternBehavior ib = js::DoNotInternAtom,
           js::FlationCoding fc = js::NormalEncoding);

extern JSAtom *
js_AtomizeChars(JSContext *cx, const jschar *chars, size_t length,
                js::InternBehavior ib = js::DoNotInternAtom);

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
js_InitAtomMap(JSContext *cx, JSAtomMap *map, js::AtomIndexMap *indices);

#endif /* jsatom_h___ */
