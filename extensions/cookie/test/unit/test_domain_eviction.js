/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

function run_test()
{
  // Set the base domain limit to 50 so we have a known value.
  Services.prefs.setIntPref("network.cookie.maxPerHost", 50);

  var cm = Cc["@mozilla.org/cookiemanager;1"].getService(Ci.nsICookieManager2);

  cm.removeAll();

  // test eviction under the 50 cookies per base domain limit. this means
  // that cookies for foo.com and bar.foo.com should count toward this limit,
  // while cookies for baz.com should not. there are several tests we perform
  // to make sure the base domain logic is working correctly.

  // 1) simplest case: set 100 cookies for "foo.bar" and make sure 50 survive.
  setCookies(cm, "foo.bar", 100);
  do_check_eq(countCookies(cm, "foo.bar", "foo.bar"), 50);

  // 2) set cookies for different subdomains of "foo.baz", and an unrelated
  // domain, and make sure all 50 within the "foo.baz" base domain are counted.
  setCookies(cm, "foo.baz", 10);
  setCookies(cm, ".foo.baz", 10);
  setCookies(cm, "bar.foo.baz", 10);
  setCookies(cm, "baz.bar.foo.baz", 10);
  setCookies(cm, "unrelated.domain", 50);
  do_check_eq(countCookies(cm, "foo.baz", "baz.bar.foo.baz"), 40);
  setCookies(cm, "foo.baz", 20);
  do_check_eq(countCookies(cm, "foo.baz", "baz.bar.foo.baz"), 50);

  // 3) ensure cookies are evicted by order of lastAccessed time, if the
  // limit on cookies per base domain is reached.
  setCookies(cm, "horse.radish", 10);

  // sleep a while, to make sure the first batch of cookies is older than
  // the second (timer resolution varies on different platforms).
  sleep(2 * 1000);
  setCookies(cm, "tasty.horse.radish", 50);
  do_check_eq(countCookies(cm, "horse.radish", "horse.radish"), 50);

  let enumerator = cm.enumerator;
  while (enumerator.hasMoreElements()) {
    let cookie = enumerator.getNext().QueryInterface(Ci.nsICookie2);

    if (cookie.host == "horse.radish")
      do_throw("cookies not evicted by lastAccessed order");
  }

  cm.removeAll();
}

// set 'aNumber' cookies with host 'aHost', with distinct names.
function
setCookies(aCM, aHost, aNumber)
{
  let expiry = (Date.now() + 1e6) * 1000;

  for (let i = 0; i < aNumber; ++i)
    aCM.add(aHost, "", "test" + i, "eviction", false, false, false, expiry);
}

// count how many cookies are within domain 'aBaseDomain', using three
// independent interface methods on nsICookieManager2:
// 1) 'enumerator', an enumerator of all cookies;
// 2) 'countCookiesFromHost', which returns the number of cookies within the
//    base domain of 'aHost',
// 3) 'getCookiesFromHost', which returns an enumerator of 2).
function
countCookies(aCM, aBaseDomain, aHost)
{
  let enumerator = aCM.enumerator;

  // count how many cookies are within domain 'aBaseDomain' using the cookie
  // enumerator.
  let cookies = [];
  while (enumerator.hasMoreElements()) {
    let cookie = enumerator.getNext().QueryInterface(Ci.nsICookie2);

    if (cookie.host.length >= aBaseDomain.length &&
        cookie.host.slice(cookie.host.length - aBaseDomain.length) == aBaseDomain)
      cookies.push(cookie);
  }

  // confirm the count using countCookiesFromHost and getCookiesFromHost.
  let result = cookies.length;
  do_check_eq(aCM.countCookiesFromHost(aBaseDomain), cookies.length);
  do_check_eq(aCM.countCookiesFromHost(aHost), cookies.length);

  enumerator = aCM.getCookiesFromHost(aHost);
  while (enumerator.hasMoreElements()) {
    let cookie = enumerator.getNext().QueryInterface(Ci.nsICookie2);

    if (cookie.host.length >= aBaseDomain.length &&
        cookie.host.slice(cookie.host.length - aBaseDomain.length) == aBaseDomain) {
      let found = false;
      for (let i = 0; i < cookies.length; ++i) {
        if (cookies[i].host == cookie.host && cookies[i].name == cookie.name) {
          found = true;
          cookies.splice(i, 1);
          break;
        }
      }

      if (!found)
        do_throw("cookie " + cookie.name + " not found in master enumerator");

    } else {
      do_throw("cookie host " + cookie.host + " not within domain " + aBaseDomain);
    }
  }

  do_check_eq(cookies.length, 0);

  return result;
}

// delay for a number of milliseconds
function sleep(delay)
{
  var start = Date.now();
  while (Date.now() < start + delay);
}

