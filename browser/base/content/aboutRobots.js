var buttonClicked = false;
var button = document.getElementById("errorTryAgain");
button.onclick = function() {
  if (buttonClicked) {
    button.style.visibility = "hidden";
  } else {
    var newLabel = button.getAttribute("label2");
    button.textContent = newLabel;
    buttonClicked = true;
  }
};
