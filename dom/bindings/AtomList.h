#ifndef mozilla_dom_AtomList_h__
#define mozilla_dom_AtomList_h__

#include "jsapi.h"
#include "mozilla/dom/GeneratedAtomList.h"

namespace mozilla {
namespace dom {

template<class T>
T* GetAtomCache(JSContext* aCx)
{
  JSRuntime* rt = JS_GetRuntime(aCx);

  auto atomCache = static_cast<PerThreadAtomCache*>(JS_GetRuntimePrivate(rt));

  return static_cast<T*>(atomCache);
}

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_AtomList_h__
