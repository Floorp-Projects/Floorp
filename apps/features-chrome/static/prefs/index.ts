export function initBeforeSessionStoreInit() {
  const prefs = Services.prefs.getDefaultBranch(null as unknown as string);

  //* Currently Noraneko's UA is
  //* Mozilla/5.0 (Windows NT 10.0; Win64; x64; rv:134.0) Gecko/20100101 Noraneko/134.0
  //* So override needed
  // https://searchfox.org/mozilla-central/rev/e24277e20c492b4a785b4488af02cca062ec7c2c/netwerk/protocol/http/nsHttpHandler.cpp#905

  //https://searchfox.org/mozilla-central/rev/e24277e20c492b4a785b4488af02cca062ec7c2c/remote/cdp/JSONHandler.sys.mjs#60

  const {userAgent} = Cc[
    "@mozilla.org/network/protocol;1?name=http"
  ].getService(Ci.nsIHttpProtocolHandler);
  prefs.setStringPref("general.useragent.override",
   userAgent.replace("Noraneko","Firefox")
  )
  prefs.setBoolPref("browser.preferences.moreFromMozilla", false);
}

export function init() {
  
}