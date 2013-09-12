// Test if isInCatchScope properly detects catch blocks.

let g = newGlobal();
let dbg = new Debugger(g);

function test(string, mustBeCaught) {
    let index = 0;
    dbg.onExceptionUnwind = function (frame) {
        let willBeCaught = false;
        do {
            if (frame.script.isInCatchScope(frame.offset)) {
                willBeCaught = true;
                break;
            }
            frame = frame.older;
        } while (frame != null);
        assertEq(willBeCaught, mustBeCaught[index++]);
    };

    try {
        g.eval(string);
    } catch (ex) {}
    assertEq(index, mustBeCaught.length);
}

// Should correctly detect catch blocks
test("throw new Error();", [false]);
test("try { throw new Error(); } catch (e) {}", [true]);
test("try { throw new Error(); } finally {}", [false, false]);
test("try { throw new Error(); } catch (e) {} finally {}", [true]);

// Source of the exception shouldn't matter
test("(null)();", [false]);
test("try { (null)(); } catch (e) {}", [true]);
test("try { (null)(); } finally {}", [false, false]);
test("try { (null)(); } catch (e) {} finally {}", [true]);

// Should correctly detect catch blocks in functions
test("function f() { throw new Error(); } f();", [false, false]);
test("function f() { try { throw new Error(); } catch (e) {} } f();", [true]);
test("function f() { try { throw new Error(); } finally {} } f();", [false, false, false]);
test("function f() { try { throw new Error(); } catch (e) {} finally {} } f();", [true]);

// Should correctly detect catch blocks in evals
test("eval('throw new Error();')", [false, false]);
test("eval('try { throw new Error(); } catch (e) {}');", [true]);
test("eval('try { throw new Error(); } finally {}');", [false, false, false]);
test("eval('try { throw new Error(); } catch (e) {} finally {}');", [true]);

// Should correctly detect rethrows
test("try { throw new Error(); } catch (e) { throw e; }", [true, false]);
test("try { try { throw new Error(); } catch (e) { throw e; } } catch (e) {}", [true, true]);
test("try { try { throw new Error(); } finally {} } catch (e) {}", [true, true]);
test("function f() { try { throw new Error(); } catch (e) { throw e; } } f();", [true, false, false]);
test("function f() { try { try { throw new Error(); } catch (e) { throw e; } } catch (e) {} } f();", [true, true]);
test("function f() { try { try { throw new Error(); } finally {} } catch (e) {} } f();", [true, true]);
test("eval('try { throw new Error(); } catch (e) { throw e; }')", [true, false, false]);
test("eval('try { try { throw new Error(); } catch (e) { throw e; } } catch (e) {}')", [true, true]);

// Should correctly detect catch blocks across frame boundaries
test("function f() { throw new Error(); } try { f(); } catch (e) {}", [true, true]);
test("function f() { throw new Error(); } try { f(); } catch (e) { throw e; }", [true, true, false]);
test("try { eval('throw new Error()'); } catch (e) {}", [true, true]);
test("try { eval('throw new Error()'); } catch (e) { throw e; }", [true, true, false]);

// Should correctly detect catch blocks just before and just after throws
test("throw new Error; try {} catch (e) {}", [false]);
test("try {} catch (e) {} throw new Error();", [false]);
