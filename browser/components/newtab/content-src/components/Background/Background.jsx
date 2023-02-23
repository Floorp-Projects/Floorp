import React, { useState } from "react";

const imgLength = 100;

export function Background(props) {
  let imgSrc = ""
  let fileImgSrc = ""
  let setFileImgSrc = ""
  let imageSrc = ""
  if(props.className == "random_image"){
    [imgSrc] = useState(`chrome://browser/skin/newtabbg-${Math.floor(Math.random() * imgLength)}.webp`);
    imageSrc =  imgSrc
  } 
  if(props.className == "selected_folder" && props.imageList != undefined){
    [fileImgSrc,setFileImgSrc] = useState(props.imageList.length != 0 ? props.imageList[Math.floor(Math.random() * props.imageList.length)] : "")
    if(props.imageList.indexOf(fileImgSrc) == -1){
      
      if(fileImgSrc != "" || props.imageList.length != 0)setFileImgSrc(props.imageList.length != 0 ? props.imageList[Math.floor(Math.random() * props.imageList.length)] : "")
      //[fileImgSrc,setFileImgSrc] = useState(props.imageList.length != 0 ? props.imageList[Math.floor(Math.random() * props.imageList.length)] : "")

    }
    imageSrc = fileImgSrc
  } 
  return <div id="background_back" className={props.className} >
  <div id="background" style={{"--background-url": `url(${imageSrc})`}} />
</div>
}
