function recurse() {
  recurse();
}

onmessage = function(event) {
  switch (event.data) {
    case "start":
      recurse();
      throw "Never should have gotten here!";
      break;
    default:
      throw "Bad message: " + event.data;
  }
}
