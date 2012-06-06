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

function animate(params, count) {
  rfa(function() {
    animateStep(params, count);
  });
}

function animateStep(params, count) {
  if (!count) {
    document.documentElement.removeAttribute("class");
    return;
  }
  var rel = document.getElementById("rel");
  for (prop in params) {
    rel.style[prop] = (parseInt(rel.style[prop]) + params[prop]) + "px";
  }
  rfa(function() {
    animateStep(params, count - 10);
  });
}
