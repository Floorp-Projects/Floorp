#include "ion/Ion.h"
#include "ion/IonFrames.h"

using namespace js;
using namespace js::ion;

IonJSFrameLayout *
InvalidationBailoutStack::fp() const
{
    return (IonJSFrameLayout *) (sp() + ionScript_->frameSize());
}

void
InvalidationBailoutStack::checkInvariants() const
{
#ifdef DEBUG
    IonJSFrameLayout *frame = fp();
    CalleeToken token = frame->calleeToken();
    JS_ASSERT(token);

    uint8 *rawBase = ionScript()->method()->raw();
    uint8 *rawLimit = rawBase + ionScript()->method()->instructionsSize();
    uint8 *osiPoint = osiPointReturnAddress();
    JS_ASSERT(rawBase <= osiPoint && osiPoint <= rawLimit);
#endif
}
