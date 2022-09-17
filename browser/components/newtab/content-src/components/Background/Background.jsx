import React, { useState } from "react";

const imgLength = 100;

export function Background() {
  const [imgSrc] = useState(`chrome://browser/skin/newtabbg-${Math.floor(Math.random() * imgLength)}.webp`);
  return <div id="background" style={{background: `url(${imgSrc})`}} />
}
