
SimpleTest.waitForExplicitFinish();

window.addEventListener("load", runTests, false);

function runTests(event)
{
    if (event.target != document) {
        return;
    }

    var elt = document.getElementById("content");

    elt.setAttribute("style", "color: blue; background-color: fuchsia");
    is(elt.style.color, "blue",
       "setting correct style attribute (color)");
    is(elt.style.backgroundColor, "fuchsia",
       "setting correct style attribute (color)");

    elt.setAttribute("style", "{color: blue; background-color: fuchsia}");
    is(elt.style.color, "",
       "setting braced style attribute (color)");
    is(elt.style.backgroundColor, "",
       "setting braced style attribute (color)");

    SimpleTest.finish();
}
