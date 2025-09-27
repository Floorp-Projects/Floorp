export function iconUrlParser(url: string): string {
  if (import.meta.env.MODE === "dev") {
    return "http://localhost:5181" + url;
  }
  return url;
}
