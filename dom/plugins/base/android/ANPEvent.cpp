#include "assert.h"
#include "ANPBase.h"
#include <android/log.h>
#include "nsThreadUtils.h"
#include "nsNPAPIPluginInstance.h"
#include "AndroidBridge.h"
#include "nsNPAPIPlugin.h"

#define LOG(args...)  __android_log_print(ANDROID_LOG_INFO, "GeckoPlugins" , ## args)
#define ASSIGN(obj, name)   (obj)->name = anp_event_##name

class PluginEventRunnable : public nsRunnable
{
public:
  PluginEventRunnable(NPP inst, ANPEvent* event, NPPluginFuncs* aFuncs)
    : mInstance(inst), mEvent(*event), mFuncs(aFuncs) {}
  virtual nsresult Run() {
    (*mFuncs->event)(mInstance, &mEvent);
    return NS_OK;
  }
private:
  NPP mInstance;
  ANPEvent mEvent;
  NPPluginFuncs* mFuncs;
};

void
anp_event_postEvent(NPP inst, const ANPEvent* event)
{
  LOG("%s", __PRETTY_FUNCTION__);
  if (!mozilla::AndroidBridge::Bridge()) {
    LOG("no bridge in %s!!!!", __PRETTY_FUNCTION__);
    return;
  }
  nsNPAPIPluginInstance* pinst = static_cast<nsNPAPIPluginInstance*>(inst->ndata);
  NPPluginFuncs* pluginFunctions = pinst->GetPlugin()->PluginFuncs();
  mozilla::AndroidBridge::Bridge()->PostToJavaThread(
    new PluginEventRunnable(inst, const_cast<ANPEvent*>(event), pluginFunctions), PR_TRUE);
  LOG("returning from %s", __PRETTY_FUNCTION__);
}


void InitEventInterface(ANPEventInterfaceV0 *i) {
  _assert(i->inSize == sizeof(*i));
  ASSIGN(i, postEvent);
}
