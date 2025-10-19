export function iconUrlParser(url: string): string {
  if (url.startsWith("data:")) {
    return url;
  }
  if (import.meta.env.DEV) {
    return "http://localhost:5181" + url;
  }
  return url;
}
