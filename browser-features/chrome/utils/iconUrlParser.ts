export function iconUrlParser(url: string): string {
  if (import.meta.env.DEV) {
    return "http://localhost:5181" + url;
  }
  return url;
}
