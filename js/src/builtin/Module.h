#ifndef Module_h___
#define Module_h___

#include "jsobj.h"

namespace js {

class Module : public JSObject {
  public:
    JSAtom *atom() {
        return &getReservedSlot(ATOM_SLOT).toString()->asAtom();
    };

    void setAtom(JSAtom *atom) {
        setReservedSlot(ATOM_SLOT, StringValue(atom));
    };

    JSScript *script() {
        return (JSScript *) getReservedSlot(SCRIPT_SLOT).toPrivate();
    }

    void setScript(JSScript *script) {
        setReservedSlot(SCRIPT_SLOT, PrivateValue(script));
    }

  private:
    static const uint32_t ATOM_SLOT = 0;
    static const uint32_t SCRIPT_SLOT = 1;
};

} // namespace js

js::Module *
js_NewModule(JSContext *cx, js::HandleAtom atom);

inline js::Module &
JSObject::asModule()
{
    JS_ASSERT(isModule());
    return *static_cast<js::Module *>(this);
}

#endif // Module_h___
