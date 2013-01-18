#include "jsobjinlines.h"
#include "builtin/Module.h"

using namespace js;

Class js::ModuleClass = {
    "Module",
    JSCLASS_HAS_RESERVED_SLOTS(2) | JSCLASS_IS_ANONYMOUS,
    JS_PropertyStub,        /* addProperty */
    JS_PropertyStub,        /* delProperty */
    JS_PropertyStub,        /* getProperty */
    JS_StrictPropertyStub,  /* setProperty */
    JS_EnumerateStub,
    JS_ResolveStub,
    JS_ConvertStub
};

Module *
js_NewModule(JSContext *cx, JSAtom *atom)
{
    RootedModule module(cx, &NewBuiltinClassInstance(cx, &ModuleClass)->asModule());
    if (module == NULL)
        return NULL;
    module->setAtom(atom);
    module->setScript(NULL);
    return module;
}
