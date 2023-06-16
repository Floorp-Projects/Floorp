onmessage = function (e) {
  let xhr = new XMLHttpRequest();
  let already_sent = false;
  xhr.addEventListener("readystatechange", event => {
    try {
      event.originalTarget.send("test");
    } catch (error) {
      if (error.name == "InvalidStateError") {
        if (!already_sent) {
          postMessage(error.name);
          already_sent = true;
        }
      }
    }
  });

  xhr.open("POST", e.data, false);
  xhr.send();
};
