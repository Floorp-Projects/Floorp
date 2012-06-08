var currentOffset = null;
var maxOffset = null;
var property = "top";

var rfa = null;
if (window.requestAnimationFrame) {
  rfa = requestAnimationFrame;
} else if (window.mozRequestAnimationFrame) {
  rfa = mozRequestAnimationFrame;
} else if (window.webkitRequestAnimationFrame) {
  rfa = webkitRequestAnimationFrame;
} else if (window.msRequestAnimationFrame) {
  rfa = msRequestAnimationFrame;
} else if (window.oRequestAnimationFrame) {
  rfa = oRequestAnimationFrame;
}

function animate(from, to, prop) {
  currentOffset = from;
  maxOffset = to;
  if (prop) {
    property = prop;
  }
  rfa(animateStep);
}

function animateStep() {
  if (currentOffset <= maxOffset) {
    document.getElementById("child").style[property] = currentOffset + "px";
    currentOffset += 10;
    rfa(animateStep);
  } else {
    document.documentElement.removeAttribute("class");
  }
}

function toAuto(prop) {
  if (prop) {
    property = prop;
  }
  rfa(setToAuto);
}

function setToAuto() {
  document.getElementById("child").style[property] = "auto";
  document.documentElement.removeAttribute("class");
}

function fromAuto(to, prop) {
  maxOffset = to;
  if (prop) {
    property = prop;
  }
  rfa(setFromAuto);
}

function setFromAuto() {
  document.getElementById("child").style[property] = maxOffset + "px";
  document.documentElement.removeAttribute("class");
}

