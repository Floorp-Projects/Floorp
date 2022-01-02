function handleRequest(request, response) {
  if (getState("reloaded") == "reloaded") {
    response.setHeader("Content-Type", "text/html; charset=iso-2022-kr", false);
    response.write("\u00E4");
  } else {
    response.setHeader("Content-Type", "text/html; charset=Shift_JIS", false);
    if (getState("loaded") == "loaded") {
      setState("reloaded", "reloaded");
    } else {
      setState("loaded", "loaded");
    }
    // kilobyte to force late-detection reload
    response.write("a".repeat(1024));
    response.write("<body>");
    response.write("\u00E4");
  }
}
