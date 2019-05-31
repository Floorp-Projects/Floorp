#include "RemoteBrowser.h"

#include "nsFrameLoader.h"
#include "nsFrameLoaderOwner.h"

namespace mozilla {
namespace dom {

RemoteBrowser* RemoteBrowser::GetFrom(nsFrameLoader* aFrameLoader) {
  if (!aFrameLoader) {
    return nullptr;
  }
  return aFrameLoader->GetRemoteBrowser();
}

RemoteBrowser* RemoteBrowser::GetFrom(nsIContent* aContent) {
  RefPtr<nsFrameLoaderOwner> loaderOwner = do_QueryObject(aContent);
  if (!loaderOwner) {
    return nullptr;
  }
  RefPtr<nsFrameLoader> frameLoader = loaderOwner->GetFrameLoader();
  return GetFrom(frameLoader);
}

}  // namespace dom
}  // namespace mozilla
