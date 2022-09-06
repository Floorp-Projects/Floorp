import React, { useState } from "react";

const imgLength = 100;

export function Background() {
  const [imgSrc] = useState(`chrome://branding/content/newtabbg-${Math.floor(Math.random() * imgLength)}.jpg`);
  return <div id="background" style={{background: `url(${imgSrc})`}} />
}
