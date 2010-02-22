// The appearance of XUL elements is platform-specific, so we set the
// style of the root element according to the platform, so that the
// CSS code inside input.css can select the correct styles for each
// platform.

var id;
var ua = navigator.userAgent;

if (/Windows/.test(ua))
  id = "win";
else if (/Linux/.test(ua))
  id = "linux";
else if (/Mac OS X/.test(ua))
  id = "mac";

if (id)
  document.documentElement.setAttribute("id", id);
else
  document.documentElement.appendChild(
    document.createTextNode("Unrecognized platform")
  );
