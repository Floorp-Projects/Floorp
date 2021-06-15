"use strict";

const { XPCShellContentUtils } = ChromeUtils.import(
  "resource://testing-common/XPCShellContentUtils.jsm"
);

XPCShellContentUtils.init(this);

const ACTOR = "AllowJavascript";

const HTML = String.raw`<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <script type="application/javascript">
    "use strict";
    var gFiredOnload = false;
    var gFiredOnclick = false;
  </script>
</head>
<body onload="gFiredOnload = true;" onclick="gFiredOnclick = true;">
</body>
</html>`;

const server = XPCShellContentUtils.createHttpServer({
  hosts: ["example.com", "example.org"],
});

server.registerPathHandler("/", (request, response) => {
  response.setHeader("Content-Type", "text/html");
  response.write(HTML);
});

function getResourceURI(file) {
  return Services.io.newFileURI(do_get_file(file)).spec;
}

const { AllowJavascriptParent } = ChromeUtils.import(
  getResourceURI("AllowJavascriptParent.jsm")
);

async function assertScriptsAllowed(bc, expectAllowed, desc) {
  let actor = bc.currentWindowGlobal.getActor(ACTOR);
  let allowed = await actor.sendQuery("CheckScriptsAllowed");
  equal(
    allowed,
    expectAllowed,
    `Scripts should be ${expectAllowed ? "" : "dis"}allowed for ${desc}`
  );
}

async function assertLoadFired(bc, expectFired, desc) {
  let actor = bc.currentWindowGlobal.getActor(ACTOR);
  let fired = await actor.sendQuery("CheckFiredLoadEvent");
  equal(
    fired,
    expectFired,
    `Should ${expectFired ? "" : "not "}have fired load for ${desc}`
  );
}

function createSubframe(bc, url) {
  let actor = bc.currentWindowGlobal.getActor(ACTOR);
  return actor.sendQuery("CreateIframe", { url });
}

add_task(async function() {
  ChromeUtils.registerWindowActor(ACTOR, {
    allFrames: true,
    child: {
      moduleURI: getResourceURI("AllowJavascriptChild.jsm"),
      events: { load: { capture: true } },
    },
    parent: {
      moduleURI: getResourceURI("AllowJavascriptParent.jsm"),
    },
  });

  let page = await XPCShellContentUtils.loadContentPage("http://example.com/", {
    remote: true,
    remoteSubframes: true,
  });

  let bc = page.browsingContext;

  {
    let oopFrame1 = await createSubframe(bc, "http://example.org/");
    let inprocFrame1 = await createSubframe(bc, "http://example.com/");

    let oopFrame1OopSub = await createSubframe(
      oopFrame1,
      "http://example.com/"
    );
    let inprocFrame1OopSub = await createSubframe(
      inprocFrame1,
      "http://example.org/"
    );

    equal(
      oopFrame1.allowJavascript,
      true,
      "OOP BC should inherit allowJavascript from parent"
    );
    equal(
      inprocFrame1.allowJavascript,
      true,
      "In-process BC should inherit allowJavascript from parent"
    );
    equal(
      oopFrame1OopSub.allowJavascript,
      true,
      "OOP BC child should inherit allowJavascript from parent"
    );
    equal(
      inprocFrame1OopSub.allowJavascript,
      true,
      "In-process child BC should inherit allowJavascript from parent"
    );

    await assertLoadFired(bc, true, "top BC");
    await assertScriptsAllowed(bc, true, "top BC");

    await assertLoadFired(oopFrame1, true, "OOP frame 1");
    await assertScriptsAllowed(oopFrame1, true, "OOP frame 1");

    await assertLoadFired(inprocFrame1, true, "In-process frame 1");
    await assertScriptsAllowed(inprocFrame1, true, "In-process frame 1");

    await assertLoadFired(oopFrame1OopSub, true, "OOP frame 1 subframe");
    await assertScriptsAllowed(oopFrame1OopSub, true, "OOP frame 1 subframe");

    await assertLoadFired(
      inprocFrame1OopSub,
      true,
      "In-process frame 1 subframe"
    );
    await assertScriptsAllowed(
      inprocFrame1OopSub,
      true,
      "In-process frame 1 subframe"
    );

    bc.allowJavascript = false;
    await assertScriptsAllowed(bc, false, "top BC with scripts disallowed");
    await assertScriptsAllowed(
      oopFrame1,
      false,
      "OOP frame 1 with top BC with scripts disallowed"
    );
    await assertScriptsAllowed(
      inprocFrame1,
      false,
      "In-process frame 1 with top BC with scripts disallowed"
    );
    await assertScriptsAllowed(
      oopFrame1OopSub,
      false,
      "OOP frame 1 subframe with top BC with scripts disallowed"
    );
    await assertScriptsAllowed(
      inprocFrame1OopSub,
      false,
      "In-process frame 1 subframe with top BC with scripts disallowed"
    );

    let oopFrame2 = await createSubframe(bc, "http://example.org/");
    let inprocFrame2 = await createSubframe(bc, "http://example.com/");

    equal(
      oopFrame2.allowJavascript,
      false,
      "OOP BC 2 should inherit allowJavascript from parent"
    );
    equal(
      inprocFrame2.allowJavascript,
      false,
      "In-process BC 2 should inherit allowJavascript from parent"
    );

    await assertLoadFired(
      oopFrame2,
      undefined,
      "OOP frame 2 with top BC with scripts disallowed"
    );
    await assertScriptsAllowed(
      oopFrame2,
      false,
      "OOP frame 2 with top BC with scripts disallowed"
    );
    await assertLoadFired(
      inprocFrame2,
      undefined,
      "In-process frame 2 with top BC with scripts disallowed"
    );
    await assertScriptsAllowed(
      inprocFrame2,
      false,
      "In-process frame 2 with top BC with scripts disallowed"
    );

    bc.allowJavascript = true;
    await assertScriptsAllowed(bc, true, "top BC");

    await assertScriptsAllowed(oopFrame1, true, "OOP frame 1");
    await assertScriptsAllowed(inprocFrame1, true, "In-process frame 1");
    await assertScriptsAllowed(oopFrame1OopSub, true, "OOP frame 1 subframe");
    await assertScriptsAllowed(
      inprocFrame1OopSub,
      true,
      "In-process frame 1 subframe"
    );

    await assertScriptsAllowed(oopFrame2, false, "OOP frame 2");
    await assertScriptsAllowed(inprocFrame2, false, "In-process frame 2");

    oopFrame1.currentWindowGlobal.allowJavascript = false;
    inprocFrame1.currentWindowGlobal.allowJavascript = false;

    await assertScriptsAllowed(
      oopFrame1,
      false,
      "OOP frame 1 with second level WC scripts disallowed"
    );
    await assertScriptsAllowed(
      inprocFrame1,
      false,
      "In-process frame 1 with second level WC scripts disallowed"
    );
    await assertScriptsAllowed(
      oopFrame1OopSub,
      false,
      "OOP frame 1 subframe second level WC scripts disallowed"
    );
    await assertScriptsAllowed(
      inprocFrame1OopSub,
      false,
      "In-process frame 1 subframe with second level WC scripts disallowed"
    );

    oopFrame1.reload(0);
    inprocFrame1.reload(0);
    await Promise.all([
      AllowJavascriptParent.promiseLoad(oopFrame1),
      AllowJavascriptParent.promiseLoad(inprocFrame1),
    ]);

    equal(
      oopFrame1.currentWindowGlobal.allowJavascript,
      true,
      "WindowContext.allowJavascript does not persist after navigation for OOP frame 1"
    );
    equal(
      inprocFrame1.currentWindowGlobal.allowJavascript,
      true,
      "WindowContext.allowJavascript does not persist after navigation for in-process frame 1"
    );

    await assertScriptsAllowed(oopFrame1, true, "OOP frame 1");
    await assertScriptsAllowed(inprocFrame1, true, "In-process frame 1");
  }

  bc.allowJavascript = false;

  bc.reload(0);
  await AllowJavascriptParent.promiseLoad(bc);

  await assertLoadFired(bc, undefined, "top BC with scripts disabled");
  await assertScriptsAllowed(bc, false, "top BC with scripts disabled");

  await page.close();
});
