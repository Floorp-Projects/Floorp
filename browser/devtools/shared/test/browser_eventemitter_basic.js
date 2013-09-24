/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

function test() {
  testEmitter();
  testEmitter({});
}


function testEmitter(aObject) {
  Cu.import("resource:///modules/devtools/shared/event-emitter.js", this);

  let emitter;

  if (aObject) {
    emitter = aObject;
    EventEmitter.decorate(emitter);
  } else {
    emitter = new EventEmitter();
  }

  ok(emitter, "We have an event emitter");

  emitter.on("next", next);
  emitter.emit("next", "abc", "def");

  let beenHere1 = false;
  function next(eventName, str1, str2) {
    is(eventName, "next", "Got event");
    is(str1, "abc", "Argument 1 is correct");
    is(str2, "def", "Argument 2 is correct");

    ok(!beenHere1, "first time in next callback");
    beenHere1 = true;

    emitter.off("next", next);

    emitter.emit("next");

    emitter.once("onlyonce", onlyOnce);

    emitter.emit("onlyonce");
    emitter.emit("onlyonce");
  }

  let beenHere2 = false;
  function onlyOnce() {
    ok(!beenHere2, "\"once\" listner has been called once");
    beenHere2 = true;
    emitter.emit("onlyonce");

    killItWhileEmitting();
  }

  function killItWhileEmitting() {
    function c1() {
      ok(true, "c1 called");
    }
    function c2() {
      ok(true, "c2 called");
      emitter.off("tick", c3);
    }
    function c3() {
      ok(false, "c3 should not be called");
    }
    function c4() {
      ok(true, "c4 called");
    }

    emitter.on("tick", c1);
    emitter.on("tick", c2);
    emitter.on("tick", c3);
    emitter.on("tick", c4);

    emitter.emit("tick");

    offAfterOnce();
  }

  function offAfterOnce() {
    let enteredC1 = false;

    function c1() {
      enteredC1 = true;
    }

    emitter.once("oao", c1);
    emitter.off("oao", c1);

    emitter.emit("oao");

    ok(!enteredC1, "c1 should not be called");

    delete emitter;
    finish();
  }
}
