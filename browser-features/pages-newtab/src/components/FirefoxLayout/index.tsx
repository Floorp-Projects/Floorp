export function FirefoxLayout() {
  return (
    <div className="flex justify-center items-center mb-8">
      <img
        src="chrome://branding/content/about-logo.png"
        alt="Logo"
        className="w-16 h-16 mr-4"
      />
      <img
        src="chrome://branding/content/firefox-wordmark.svg"
        alt="Wordmark"
        className="h-12"
      />
    </div>
  );
}
