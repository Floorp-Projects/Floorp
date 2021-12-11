let testframe = document.getElementById("testframe");
testframe.onload = function() {
  parent.postMessage(
    {
      result: "frame-allowed",
      href: document.location.href,
    },
    "*"
  );
};
testframe.onerror = function() {
  parent.postMessage(
    {
      result: "frame-blocked",
      href: document.location.href,
    },
    "*"
  );
};
testframe.src = "file_frame_src_inner.html";
