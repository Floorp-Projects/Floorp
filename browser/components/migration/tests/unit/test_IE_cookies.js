XPCOMUtils.defineLazyModuleGetter(this, "ctypes",
                                  "resource://gre/modules/ctypes.jsm");

add_task(function* () {
  let migrator = MigrationUtils.getMigrator("ie");
  // Sanity check for the source.
  Assert.ok(migrator.sourceExists);

  const BOOL = ctypes.bool;
  const LPCTSTR = ctypes.char16_t.ptr;

  let wininet = ctypes.open("Wininet");
  do_register_cleanup(() => {
    try {
      wininet.close();
    } catch (ex) {}
  });

  /*
  BOOL InternetSetCookie(
    _In_  LPCTSTR lpszUrl,
    _In_  LPCTSTR lpszCookieName,
    _In_  LPCTSTR lpszCookieData
  );
  */
  let setIECookie = wininet.declare("InternetSetCookieW",
                                    ctypes.default_abi,
                                    BOOL,
                                    LPCTSTR,
                                    LPCTSTR,
                                    LPCTSTR);

  let expiry = new Date();
  expiry.setDate(expiry.getDate() + 7);
  const COOKIE = {
    host: "mycookietest.com",
    name: "testcookie",
    value: "testvalue",
    expiry
  };

  // Sanity check.
  Assert.equal(Services.cookies.countCookiesFromHost(COOKIE.host), 0,
               "There are no cookies initially");

  // Create the persistent cookie in IE.
  let value = COOKIE.name + " = " + COOKIE.value +"; expires = " +
              COOKIE.expiry.toUTCString();
  let rv = setIECookie(new URL("http://" + COOKIE.host).href, null, value);
  Assert.ok(rv, "Added a persistent IE cookie");

  // Migrate cookies.
  yield promiseMigration(migrator, MigrationUtils.resourceTypes.COOKIES);

  Assert.equal(Services.cookies.countCookiesFromHost(COOKIE.host), 1,
               "Migrated the expected number of cookies");

  // Now check the cookie details.
  let enumerator = Services.cookies.getCookiesFromHost(COOKIE.host);
  Assert.ok(enumerator.hasMoreElements());
  let foundCookie = enumerator.getNext().QueryInterface(Ci.nsICookie2);

  Assert.equal(foundCookie.name, COOKIE.name);
  Assert.equal(foundCookie.value, COOKIE.value);
  Assert.equal(foundCookie.host, "." + COOKIE.host);
  Assert.equal(foundCookie.expiry, Math.floor(COOKIE.expiry / 1000));
});
