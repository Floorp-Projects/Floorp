/* Any copyright is dedicated to the Public Domain.
 * https://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Insert URLs into Internet Explorer (IE) history so we can test importing
 * them.
 *
 * See API docs at
 * https://docs.microsoft.com/en-us/previous-versions/windows/internet-explorer/ie-developer/platform-apis/ms774949(v%3dvs.85)
 */

#include <urlhist.h>  // IUrlHistoryStg
#include <shlguid.h>  // SID_SUrlHistory

int main(int argc, char** argv) {
  HRESULT hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
  if (FAILED(hr)) {
    CoUninitialize();
    return -1;
  }
  IUrlHistoryStg* ieHist;

  hr =
      ::CoCreateInstance(CLSID_CUrlHistory, nullptr, CLSCTX_INPROC_SERVER,
                         IID_IUrlHistoryStg, reinterpret_cast<void**>(&ieHist));
  if (FAILED(hr)) return -2;

  hr = ieHist->AddUrl(L"http://www.mozilla.org/1", L"Mozilla HTTP Test", 0);
  if (FAILED(hr)) return -3;

  hr =
      ieHist->AddUrl(L"https://www.mozilla.org/2", L"Mozilla HTTPS Test 🦊", 0);
  if (FAILED(hr)) return -4;

  CoUninitialize();

  return 0;
}
