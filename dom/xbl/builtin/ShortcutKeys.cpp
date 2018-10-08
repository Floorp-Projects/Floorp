#include "mozilla/ShortcutKeys.h"
#include "../nsXBLPrototypeHandler.h"
#include "nsContentUtils.h"

namespace mozilla {

NS_IMPL_ISUPPORTS(ShortcutKeys, nsIObserver);

StaticRefPtr<ShortcutKeys> ShortcutKeys::sInstance;

ShortcutKeys::ShortcutKeys()
  : mBrowserHandlers(nullptr)
  , mEditorHandlers(nullptr)
  , mInputHandlers(nullptr)
  , mTextAreaHandlers(nullptr)
{
  MOZ_ASSERT(!sInstance, "Attempt to instantiate a second ShortcutKeys.");
  nsContentUtils::RegisterShutdownObserver(this);
}

ShortcutKeys::~ShortcutKeys()
{
  delete mBrowserHandlers;
  delete mEditorHandlers;
  delete mInputHandlers;
  delete mTextAreaHandlers;
}

nsresult
ShortcutKeys::Observe(nsISupports* aSubject, const char* aTopic, const char16_t* aData)
{
  // Clear our strong reference so we can clean up.
  sInstance = nullptr;
  return NS_OK;
}

/* static */ nsXBLPrototypeHandler*
ShortcutKeys::GetHandlers(HandlerType aType)
{
  if (!sInstance) {
    sInstance = new ShortcutKeys();
  }

  return sInstance->EnsureHandlers(aType);
}

nsXBLPrototypeHandler*
ShortcutKeys::EnsureHandlers(HandlerType aType)
{
  ShortcutKeyData* keyData;
  nsXBLPrototypeHandler** cache;

  switch (aType) {
    case HandlerType::eBrowser:
      keyData = &sBrowserHandlers[0];
      cache = &mBrowserHandlers;
      break;
    case HandlerType::eEditor:
      keyData = &sEditorHandlers[0];
      cache = &mEditorHandlers;
      break;
    case HandlerType::eInput:
      keyData = &sInputHandlers[0];
      cache = &mInputHandlers;
      break;
    case HandlerType::eTextArea:
      keyData = &sTextAreaHandlers[0];
      cache = &mTextAreaHandlers;
      break;
    default:
      MOZ_ASSERT(false, "Unknown handler type requested.");
  }

  if (*cache) {
    return *cache;
  }

  nsXBLPrototypeHandler* lastHandler = nullptr;
  while (keyData->event) {
    nsXBLPrototypeHandler* handler =
      new nsXBLPrototypeHandler(keyData);
    if (lastHandler) {
      lastHandler->SetNextHandler(handler);
    } else {
      *cache = handler;
    }
    lastHandler = handler;
    keyData++;
  }

  return *cache;
}

} // namespace mozilla
