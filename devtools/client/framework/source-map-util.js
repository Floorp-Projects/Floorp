function originalToGeneratedId(originalId) {
  const match = originalId.match(/(.*)\/originalSource/);
  return match ? match[1] : "";
}

function generatedToOriginalId(generatedId, url) {
  return generatedId + "/originalSource-" + url.replace(/ \//, '-');
}

function isOriginalId(id) {
  return !!id.match(/\/originalSource/);
}

function isGeneratedId(id) {
  return !isOriginalId(id);
}

module.exports = {
  originalToGeneratedId, generatedToOriginalId, isOriginalId, isGeneratedId
};
