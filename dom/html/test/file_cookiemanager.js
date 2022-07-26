/* eslint-env mozilla/chrome-script */

addMessageListener("getCookieFromManager", ({ host, path }) => {
  let cm = Cc["@mozilla.org/cookiemanager;1"].getService(Ci.nsICookieManager);
  let values = [];
  path = path.substring(0, path.lastIndexOf("/"));
  for (let cookie of cm.cookies) {
    if (!cookie) {
      break;
    }
    if (host != cookie.host || path != cookie.path) {
      continue;
    }
    values.push(cookie.name + "=" + cookie.value);
  }

  sendAsyncMessage("getCookieFromManager:return", {
    cookie: values.join("; "),
  });
});
