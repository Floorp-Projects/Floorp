#include "jsobjinlines.h"
#include "builtin/Module.h"

using namespace js;

Class js::ModuleClass = {
    "Module",
    JSCLASS_HAS_RESERVED_SLOTS(2) | JSCLASS_IS_ANONYMOUS,
    JS_PropertyStub,        /* addProperty */
    JS_DeletePropertyStub,  /* delProperty */
    JS_PropertyStub,        /* getProperty */
    JS_StrictPropertyStub,  /* setProperty */
    JS_EnumerateStub,
    JS_ResolveStub,
    JS_ConvertStub
};

Module *
js_NewModule(JSContext *cx, HandleAtom atom)
{
    RootedObject object(cx, NewBuiltinClassInstance(cx, &ModuleClass));
    if (!object)
        return NULL;
    RootedModule module(cx, &object->asModule());
    module->setAtom(atom);
    module->setScript(NULL);
    return module;
}
