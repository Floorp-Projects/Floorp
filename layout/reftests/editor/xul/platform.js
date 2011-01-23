// The appearance of XUL elements is platform-specific, so we set the
// style of the root element according to the platform, so that the
// CSS code inside input.css can select the correct styles for each
// platform.

var id;
var ua = navigator.userAgent;

if (/Windows/.test(ua)) {
  id = "win";
  if (/NT 5\.1/.test(ua) || /NT 5\.2; Win64/.test(ua))
    var classname = "winxp";
}
else if (/Linux/.test(ua))
  id = "linux";
else if (/SunOS/.test(ua))
  id = "linux";
else if (/Mac OS X/.test(ua))
  id = "mac";

if (id)
  document.documentElement.setAttribute("id", id);
else
  document.documentElement.appendChild(
    document.createTextNode("Unrecognized platform")
  );
if (classname)
  document.documentElement.setAttribute("class", classname);
