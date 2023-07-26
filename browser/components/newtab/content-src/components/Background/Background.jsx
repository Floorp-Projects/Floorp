import React, { useState } from "react";

const imgLength = 100;

export function Background(props) {
  
  if(props.className == "random_image"){
    let [imgSrc,setImgSrc] = useState({
      "url":`chrome://browser/skin/newtabbg-${Math.floor(Math.random() * imgLength)}.webp`
    }) 
    if(!imgSrc.url.startsWith("chrome://browser/skin/newtabbg-")){
      setImgSrc({
        "url":`chrome://browser/skin/newtabbg-${Math.floor(Math.random() * imgLength)}.webp`
      })
    }
    return <div id="background_back" className={props.className} >
    <div id="background" style={{"--background-url": `url(${imgSrc.url})`}} />
  </div>
  } else if(props.className == "selected_folder" && props.imageList != undefined){
    let [fileImgSrc,setFileImgSrc] = useState({
      "url":props.imageList.urls.length != 0 ? props.imageList.urls[Math.floor(Math.random() * props.imageList.urls.length)] : ""
    })
    if(props.imageList.urls.length != 0){
      if(props.imageList.urls.indexOf(fileImgSrc.url) == -1 || props.pref["floorpBackgroundPathsVal_" + fileImgSrc.url]?.data === null){
        fileImgSrc.url = props.imageList.urls.length != 0 ? props.imageList.urls[Math.floor(Math.random() * props.imageList.urls.length)] : ""
        setFileImgSrc({
          "url":fileImgSrc.url
        })
        if("blobData" in fileImgSrc) delete fileImgSrc.blobData
      }
      if("data" in fileImgSrc){
        return <div id="background_back" className={props.className} >
  <div id="background" style={{"--background-url": `url(${fileImgSrc.data})`}} />
</div>
      }else if(props.pref["floorpBackgroundPathsVal_" + fileImgSrc.url]?.data != undefined){
        setImgData(props.pref["floorpBackgroundPathsVal_" + fileImgSrc.url].data,fileImgSrc.url,props.pref["floorpBackgroundPathsVal_" + fileImgSrc.url].type,setFileImgSrc)
      }else{
        props.getImg(fileImgSrc.url)
      }

      return <div id="background_back" className={props.className} >
  <div id="background" style={{"--background-url": `url(${fileImgSrc.data})`}} />
</div>
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
    "data":blobURL
  })
}