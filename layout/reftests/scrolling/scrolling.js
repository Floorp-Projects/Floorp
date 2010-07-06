var topElements = document.getElementsByClassName("scrollTop");
if (!topElements.length) {
  topElements = [document.body];
}

function doScroll(d)
{
  for (var i = 0; i < topElements.length; ++i) {
    topElements[i].scrollTop = d;
  }
}

if (document.location.search == '?ref') {
  doScroll(20);
} else {
  doScroll(1);
  document.documentElement.setAttribute("class", "reftest-wait");
  window.addEventListener("MozReftestInvalidate", function() {
    document.documentElement.removeAttribute("class");
    doScroll(20);
  }, false);
}
