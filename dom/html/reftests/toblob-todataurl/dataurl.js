function init() {
  function end() {
    document.documentElement.className = '';
  }

  function next() {
    compressAndDisplay(original, end);
  }

  var original = getImageFromDataUrl(sample);
  setImgLoadListener(original, next);
}

function compressAndDisplay(image, next) {
  var canvas = document.createElement('canvas');
  canvas.width = image.naturalWidth;
  canvas.height = image.naturalHeight;
  var ctx = canvas.getContext('2d');
  ctx.drawImage(image, 0, 0);

  var dataUrl;
  // I want to test passing undefined as well
  if ('quality' in window) {
    dataUrl = canvas.toDataURL('image/jpeg', quality);
  } else {
    dataUrl = canvas.toDataURL('image/jpeg');
  }

  var img = getImageFromDataUrl(dataUrl);
  setImgLoadListener(img, next);
  document.body.appendChild(img);
}

function setImgLoadListener(img, func) {
  if (img.complete) {
    func.call(img, { target: img});
  } else {
    img.addEventListener('load', func);
  }
}

function naturalDimensionsHandler(e) {
  var img = e.target;
  img.width = img.naturalWidth;
  img.height = img.naturalHeight;
}

function getImageFromDataUrl(url) {
  var img = document.createElement('img');
  img.src = url;
  setImgLoadListener(img, naturalDimensionsHandler);

  return img;
}

init();
