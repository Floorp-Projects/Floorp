export function safeURI(url) {
  if (!url) {
    return "";
  }
  const {protocol} = new URL(url);
  const isAllowed = [
    "http:",
    "https:",
    "data:",
    "resource:",
    "chrome:"
  ].includes(protocol);
  if (!isAllowed) {
    console.warn(`The protocol ${protocol} is not allowed for template URLs.`); // eslint-disable-line no-console
  }
  return isAllowed ? url : "";
}
