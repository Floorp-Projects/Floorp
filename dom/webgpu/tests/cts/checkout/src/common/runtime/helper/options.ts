let windowURL: URL | undefined = undefined;
function getWindowURL() {
  if (windowURL === undefined) {
    windowURL = new URL(window.location.toString());
  }
  return windowURL;
}

export function optionEnabled(
  opt: string,
  searchParams: URLSearchParams = getWindowURL().searchParams
): boolean {
  const val = searchParams.get(opt);
  return val !== null && val !== '0';
}

export function optionString(
  opt: string,
  searchParams: URLSearchParams = getWindowURL().searchParams
): string {
  return searchParams.get(opt) || '';
}
