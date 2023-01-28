import React, { useState } from "react";

const imgLength = 100;

export function Background(props) {
  let imgSrc = ""
  if(props.className == "random_image") [imgSrc] = useState(`chrome://browser/skin/newtabbg-${Math.floor(Math.random() * imgLength)}.webp`);
  if(props.className == "selected_folder" && props.imageList != undefined) imgSrc = props.imageList.length != 0 ? props.imageList[Math.floor(Math.random() * props.imageList.length)] : ""

  return <div id="background_back" className={props.className} >
  <div id="background" style={{"--background-url": `url(${imgSrc})`}} />
</div>
}
