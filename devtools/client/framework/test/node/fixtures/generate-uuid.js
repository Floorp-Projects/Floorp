function generateUUID() {
  return `${Date.now()}-${Math.round(Math.random() * 100)}`;
}

module.exports = { generateUUID };
