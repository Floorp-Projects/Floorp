// Type declarations for @nora/skin ?raw imports
// These are Vite-specific imports that resolve at build time
// For Deno type checking, we declare them as strings

declare module "@nora/skin" {
  export const lepton: string;
  export const photon: string;
  export const protonfix: string;
}

declare module "@nora/skin/lepton/userjs/lepton.js?raw" {
  const content: string;
  export default content;
}

declare module "@nora/skin/lepton/userjs/photon.js?raw" {
  const content: string;
  export default content;
}

declare module "@nora/skin/lepton/userjs/protonfix.js?raw" {
  const content: string;
  export default content;
}

declare module "@nora/skin/lepton/css/leptonChrome.css?raw" {
  const content: string;
  export default content;
}

declare module "@nora/skin/lepton/css/leptonContent.css?raw" {
  const content: string;
  export default content;
}

declare module "@nora/skin/fluerial/css/fluerial.css?raw" {
  const content: string;
  export default content;
}

declare module "@nora/skin/*" {
  const content: string;
  export default content;
}
