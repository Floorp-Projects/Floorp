var topElements = document.getElementsByClassName("scrollTop");
if (!topElements.length) {
  topElements = [document.documentElement];
}

var failed = false;

function doScroll(d)
{
  if (failed)
    return;
  for (var i = 0; i < topElements.length; ++i) {
    var e = topElements[i];
    e.scrollTop = d;
    if (e.scrollTop != d) {
      document.documentElement.textContent =
          "Scrolling failed on " + e.tagName + " element, " +
          "tried to scroll to " + d + ", got " + e.scrollTop +
          " (Random number: " + Math.random() + ")";
      failed = true;
    }
  }
}

if (document.location.search == '?ref') {
  doScroll(20);
} else if (document.location.search == '?up') {
  doScroll(40);
  document.documentElement.setAttribute("class", "reftest-wait");
  window.addEventListener("MozReftestInvalidate", function() {
    document.documentElement.removeAttribute("class");
    doScroll(20);
  }, false);
} else {
  doScroll(1);
  document.documentElement.setAttribute("class", "reftest-wait");
  window.addEventListener("MozReftestInvalidate", function() {
    document.documentElement.removeAttribute("class");
    doScroll(20);
  }, false);
}
