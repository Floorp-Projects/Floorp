export function truncateText(text = "", cap) {
  return text.substring(0, cap).trim() + (text.length > cap ? "…" : "");
}
