"use strict";

XPCOMUtils.defineLazyModuleGetter(this, "ctypes",
                                  "resource://gre/modules/ctypes.jsm");

add_task(async function() {
  let migrator = MigrationUtils.getMigrator("ie");
  // Sanity check for the source.
  Assert.ok(migrator.sourceExists);

  const BOOL = ctypes.bool;
  const LPCTSTR = ctypes.char16_t.ptr;
  const DWORD = ctypes.uint32_t;
  const LPDWORD = DWORD.ptr;

  let wininet = ctypes.open("Wininet");

  /*
  BOOL InternetSetCookieW(
    _In_  LPCTSTR lpszUrl,
    _In_  LPCTSTR lpszCookieName,
    _In_  LPCTSTR lpszCookieData
  );
  */
  // NOTE: Even though MSDN documentation does not indicate a calling convention,
  // InternetSetCookieW is declared in SDK headers as __stdcall but is exported
  // from wininet.dll without name mangling, so it is effectively winapi_abi
  let setIECookie = wininet.declare("InternetSetCookieW",
                                    ctypes.winapi_abi,
                                    BOOL,
                                    LPCTSTR,
                                    LPCTSTR,
                                    LPCTSTR);

  /*
  BOOL InternetGetCookieW(
    _In_    LPCTSTR lpszUrl,
    _In_    LPCTSTR lpszCookieName,
    _Out_   LPCTSTR  lpszCookieData,
    _Inout_ LPDWORD lpdwSize
  );
  */
  // NOTE: Even though MSDN documentation does not indicate a calling convention,
  // InternetGetCookieW is declared in SDK headers as __stdcall but is exported
  // from wininet.dll without name mangling, so it is effectively winapi_abi
  let getIECookie = wininet.declare("InternetGetCookieW",
                                    ctypes.winapi_abi,
                                    BOOL,
                                    LPCTSTR,
                                    LPCTSTR,
                                    LPCTSTR,
                                    LPDWORD);

  // We need to randomize the cookie to avoid clashing with other cookies
  // that might have been set by previous tests and not properly cleared.
  let date = (new Date()).getDate();
  const COOKIE = {
    get host() {
      return new URL(this.href).host;
    },
    href: `http://mycookietest.${Math.random()}.com`,
    name: "testcookie",
    value: "testvalue",
    expiry: new Date(new Date().setDate(date + 2))
  };
  let data = ctypes.char16_t.array()(256);
  let sizeRef = DWORD(256).address();

  registerCleanupFunction(() => {
    // Remove the cookie.
    try {
      let expired = new Date(new Date().setDate(date - 2));
      let rv = setIECookie(COOKIE.href, COOKIE.name,
                           `; expires=${expired.toUTCString()}`);
      Assert.ok(rv, "Expired the IE cookie");
      Assert.ok(!getIECookie(COOKIE.href, COOKIE.name, data, sizeRef),
      "The cookie has been properly removed");
    } catch (ex) {}

    // Close the library.
    try {
      wininet.close();
    } catch (ex) {}
  });

  // Create the persistent cookie in IE.
  let value = `${COOKIE.value}; expires=${COOKIE.expiry.toUTCString()}`;
  let rv = setIECookie(COOKIE.href, COOKIE.name, value);
  Assert.ok(rv, "Added a persistent IE cookie: " + value);

  // Sanity check the cookie has been created.
  Assert.ok(getIECookie(COOKIE.href, COOKIE.name, data, sizeRef),
            "Found the added persistent IE cookie");
  info("Found cookie: " + data.readString());
  Assert.equal(data.readString(), `${COOKIE.name}=${COOKIE.value}`,
            "Found the expected cookie");

  // Sanity check that there are no cookies.
  Assert.equal(Services.cookies.countCookiesFromHost(COOKIE.host), 0,
               "There are no cookies initially");

  // Migrate cookies.
  await promiseMigration(migrator, MigrationUtils.resourceTypes.COOKIES);

  Assert.equal(Services.cookies.countCookiesFromHost(COOKIE.host), 1,
               "Migrated the expected number of cookies");

  // Now check the cookie details.
  let enumerator = Services.cookies.getCookiesFromHost(COOKIE.host, {});
  Assert.ok(enumerator.hasMoreElements());
  let foundCookie = enumerator.getNext().QueryInterface(Ci.nsICookie2);

  Assert.equal(foundCookie.name, COOKIE.name);
  Assert.equal(foundCookie.value, COOKIE.value);
  Assert.equal(foundCookie.host, "." + COOKIE.host);
  Assert.equal(foundCookie.expiry, Math.floor(COOKIE.expiry / 1000));
});
