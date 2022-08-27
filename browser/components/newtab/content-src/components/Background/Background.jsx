import React, { useState } from "react";
import images from "./images.json";

const imgOrigin = "https://images.unsplash.com/";

export function Background() {
  const [imgSrc, setImgSrc] = useState(imgOrigin + images[Math.floor(Math.random() * images.length)]);
  return <div id="background" style={{backgroundImage: `url(${imgSrc})`}} />
}
