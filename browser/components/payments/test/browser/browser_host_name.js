"use strict";

async function withBasicRequestDialogForOrigin(origin, dialogTaskFn) {
  const args = {
    methodData: [PTU.MethodData.basicCard],
    details: PTU.Details.total60USD,
  };
  await spawnInDialogForMerchantTask(PTU.ContentTasks.createAndShowRequest, dialogTaskFn, args, {
    origin,
  });
}

add_task(async function test_host() {
  await withBasicRequestDialogForOrigin("https://example.com", () => {
    is(content.document.querySelector("#host-name").textContent, "example.com",
       "Check basic host name");
  });
});

add_task(async function test_host_subdomain() {
  await withBasicRequestDialogForOrigin("https://test1.example.com", () => {
    is(content.document.querySelector("#host-name").textContent, "test1.example.com",
       "Check host name with subdomain");
  });
});

add_task(async function test_host_IDN() {
  await withBasicRequestDialogForOrigin("https://xn--hxajbheg2az3al.xn--jxalpdlp", () => {
    is(content.document.querySelector("#host-name").textContent,
       "\u03C0\u03B1\u03C1\u03AC\u03B4\u03B5\u03B9\u03B3\u03BC\u03B1." +
       "\u03B4\u03BF\u03BA\u03B9\u03BC\u03AE",
       "Check IDN domain");
  });
});
