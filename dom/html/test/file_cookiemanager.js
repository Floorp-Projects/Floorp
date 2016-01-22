let { classes: Cc, interfaces: Ci } = Components;
addMessageListener("getCookieFromManager", ({ host, path }) => {
  let cm = Cc["@mozilla.org/cookiemanager;1"]
             .getService(Ci.nsICookieManager);
  let values = [];
  path = path.substring(0, path.lastIndexOf("/") + 1);
  let e = cm.enumerator;
  while (e.hasMoreElements()) {
    let cookie = e.getNext().QueryInterface(Ci.nsICookie);
    if (!cookie) {
      break;
    }
    if (host != cookie.host || path != cookie.path) {
      continue;
    }
    values.push(cookie.name + "=" + cookie.value);
  }

  sendAsyncMessage("getCookieFromManager:return", { cookie: values.join("; ") });
});
