/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

const { CookieXPCShellUtils } = ChromeUtils.import(
  "resource://testing-common/CookieXPCShellUtils.jsm"
);

CookieXPCShellUtils.init(this);

// Same agent cluster, all works fine: blobURLs can be opened.
add_task(async () => {
  do_get_profile();

  // CookieXPCShellUtils.createServer does not support https
  Services.prefs.setBoolPref("dom.security.https_first", false);

  Services.prefs.setBoolPref(
    "privacy.partition.bloburl_per_agent_cluster",
    true
  );

  const server = CookieXPCShellUtils.createServer({ hosts: ["example.org"] });

  let result = new Promise(resolve => {
    server.registerPathHandler("/result", (metadata, response) => {
      resolve(metadata.queryString == "ok");

      const body = "Done";
      response.bodyOutputStream.write(body, body.length);
    });
  });

  server.registerPathHandler("/test", (metadata, response) => {
    response.setStatusLine(metadata.httpVersion, 200, "OK");
    response.setHeader("Content-Type", "text/html", false);
    const body = `<script>
      let b = new Blob(["Hello world!"]);
      let u = URL.createObjectURL(b);
      fetch(u).then(r => r.text()).then(t => {
         if (t !== "Hello world!") {
           throw new Error(42);
         }
      }).then(() => fetch("/result?ok"), () => fetch("/result?failure")).then(() => {});
    </script>`;
    response.bodyOutputStream.write(body, body.length);
  });

  let contentPage = await CookieXPCShellUtils.loadContentPage(
    "http://example.org/test"
  );

  Assert.ok(await result, "BlobURL works");
  await contentPage.close();
});

// Same agent cluster: frames
add_task(async () => {
  do_get_profile();

  // CookieXPCShellUtils.createServer does not support https
  Services.prefs.setBoolPref("dom.security.https_first", false);

  const server = CookieXPCShellUtils.createServer({ hosts: ["example.org"] });

  let result = new Promise(resolve => {
    server.registerPathHandler("/result", (metadata, response) => {
      resolve(metadata.queryString == "ok");

      const body = "Done";
      response.bodyOutputStream.write(body, body.length);
    });
  });

  server.registerPathHandler("/iframe", (metadata, response) => {
    response.setStatusLine(metadata.httpVersion, 200, "OK");
    response.setHeader("Content-Type", "text/html", false);
    const body = `<script>
      fetch("${metadata.queryString}").then(r => r.text()).then(t => {
         if (t !== "Hello world!") {
           throw new Error(42);
         }
      }).then(() => fetch("/result?ok"), () => fetch("/result?failure")).then(() => {});
    </script>`;
    response.bodyOutputStream.write(body, body.length);
  });

  server.registerPathHandler("/test", (metadata, response) => {
    response.setStatusLine(metadata.httpVersion, 200, "OK");
    response.setHeader("Content-Type", "text/html", false);
    const body = `<iframe id="a"></iframe><script>
      let b = new Blob(["Hello world!"]);
      let u = URL.createObjectURL(b);
      document.getElementById("a").src = "/iframe?" + u;
    </script>`;
    response.bodyOutputStream.write(body, body.length);
  });

  let contentPage = await CookieXPCShellUtils.loadContentPage(
    "http://example.org/test"
  );

  Assert.ok(await result, "BlobURL works");
  await contentPage.close();
});

// Cross agent cluster: different tabs
add_task(async () => {
  do_get_profile();

  const server = CookieXPCShellUtils.createServer({ hosts: ["example.org"] });

  let result = new Promise(resolve => {
    server.registerPathHandler("/result", (metadata, response) => {
      resolve(metadata.queryString == "ok");

      const body = "Done";
      response.bodyOutputStream.write(body, body.length);
    });
  });

  const step = new Promise(resolve => {
    server.registerPathHandler("/step", (metadata, response) => {
      resolve(metadata.queryString);
      response.setStatusLine(metadata.httpVersion, 200, "OK");
      response.setHeader("Content-Type", "text/html", false);
      const body = "Thanks!";
      response.bodyOutputStream.write(body, body.length);
    });
  });

  server.registerPathHandler("/test", (metadata, response) => {
    response.setStatusLine(metadata.httpVersion, 200, "OK");
    response.setHeader("Content-Type", "text/html", false);
    const body = `<script>
      let b = new Blob(["Hello world!"]);
      let u = URL.createObjectURL(b);
      fetch("/step?" + u).then(() => {});
    </script>`;
    response.bodyOutputStream.write(body, body.length);
  });

  let contentPage = await CookieXPCShellUtils.loadContentPage(
    "http://example.org/test"
  );

  const blobURL = await step;
  Assert.ok(blobURL.length, "We have a blobURL");

  server.registerPathHandler("/cross-test", (metadata, response) => {
    response.setStatusLine(metadata.httpVersion, 200, "OK");
    response.setHeader("Content-Type", "text/html", false);
    const body = `<script>
      fetch("${metadata.queryString}").then(r => r.text()).then(t => {
         if (t !== "Hello world!") {
           throw new Error(42);
         }
      }).then(() => fetch("/result?ok"), () => fetch("/result?failure")).then(() => {});
    </script>`;
    response.bodyOutputStream.write(body, body.length);
  });

  let contentPage2 = await CookieXPCShellUtils.loadContentPage(
    "http://example.org/cross-test?" + blobURL
  );

  Assert.ok(!(await result), "BlobURL should not work");
  await contentPage.close();
  await contentPage2.close();
});
