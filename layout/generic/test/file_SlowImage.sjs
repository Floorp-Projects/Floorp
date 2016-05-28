"use strict";
const Cc = Components.classes;
const Ci = Components.interfaces;

const IMG_BYTES = atob(
  "iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAYAAAAfFcSJAAAA" +
  "DUlEQVQImWNgY2P7DwABOgESJhRQtgAAAABJRU5ErkJggg==");

function handleRequest(request, response) {
  response.processAsync();
  getObjectState("context", function(obj) {
    let ctx;
    if (obj == null) {
      ctx = {
        QueryInterface: function(iid) {
          if (iid.equals(Components.interfaces.nsISupports))
            return this;
          throw Components.results.NS_ERROR_NO_INTERFACE;
        }
      };
      ctx.wrappedJSObject = ctx;

      ctx.promise = new Promise(resolve => {
        ctx.resolve = resolve;
      });

      setObjectState("context", ctx);
    } else {
      ctx = obj.wrappedJSObject;
    }
    Promise.resolve(ctx).then(next);
  });

  function next(ctx) {
    if (request.queryString.indexOf("continue") >= 0) {
      ctx.resolve();
    }

    ctx.promise.then(() => {
      response.setHeader("Content-Type", "image/png");
      response.write(IMG_BYTES);
      response.finish();
    });
  }
}
