import React, { useState } from "react";

const imgLength = 100;

export function Background(props) {
  
  if(props.className == "random_image"){
    let [imgSrc] = useState(`chrome://browser/skin/newtabbg-${Math.floor(Math.random() * imgLength)}.webp`);
    return <div id="background_back" className={props.className} >
    <div id="background" style={{"--background-url": `url(${imgSrc})`}} />
  </div>
  } 
  console.log(props.imageList)
  if(props.className == "selected_folder" && props.imageList != undefined){
    let [fileImgSrc,setFileImgSrc] = useState({
      "url":props.imageList.urls.length != 0 ? props.imageList.urls[Math.floor(Math.random() * props.imageList.urls.length)] : ""
    })
    if(props.imageList.urls.length != 0){
      if(props.imageList.urls.indexOf(fileImgSrc.url) == -1){
        fileImgSrc.url = props.imageList.urls.length != 0 ? props.imageList.urls[Math.floor(Math.random() * props.imageList.urls.length)] : ""
        if("blobData" in fileImgSrc) delete fileImgSrc.blobData
      }
      if(!("blobData" in fileImgSrc)){
        setImgData(props.imageList.data[fileImgSrc.url].data,fileImgSrc.url,props.imageList.data[fileImgSrc.url].type,setFileImgSrc)
      }else{
        return <div id="background_back" className={props.className} >
  <div id="background" style={{"--background-url": `url(${fileImgSrc.blobData})`}} />
</div>
      }
    }else if(fileImgSrc.url != ""){
      setFileImgSrc({"url":""})
    }

  } 
  return <div id="background_back" className={props.className} >
  <div id="background" />
</div>
}

async function setImgData(data,url,type,result){
  let blobURL = URL.createObjectURL(new Blob([data], { type: type }))
  result({
    "url":url,
    "blobData":blobURL
  })
}