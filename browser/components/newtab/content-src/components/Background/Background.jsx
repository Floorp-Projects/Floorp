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
  } else if(props.className == "selected_folder" && props.pref?.backgroundPaths){
    const imageList = props.pref.backgroundPaths
    let [fileImgSrc,setFileImgSrc] = useState({
      "url":imageList.urls.length != 0 ? imageList.urls[Math.floor(Math.random() * imageList.urls.length)] : ""
    })
    if(imageList.urls.length != 0){
      if(imageList.urls.indexOf(fileImgSrc.url) == -1 || props.pref["floorpBackgroundPathsVal_" + fileImgSrc.url]?.data === null){
        fileImgSrc.url = imageList.urls.length != 0 ? imageList.urls[Math.floor(Math.random() * imageList.urls.length)] : ""
        setFileImgSrc({
          "url":fileImgSrc.url
        })
      }
      if("data" in fileImgSrc){
        return <div id="background_back" className={props.className} >
  <div id="background" style={{"--background-url": `url(${fileImgSrc.data})`}} />
</div>
      }else if(props.pref["floorpBackgroundPathsVal_" + fileImgSrc.url]?.data){
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

  } else if(props.className == "selected_image" && props.pref.oneImageData){
    const imgData = props.pref.oneImageData
    let [fileImgSrc,setFileImgSrc] = useState({
      "url":imgData.url ?? ""
    })
    console.log(fileImgSrc)
    if(imgData.url){
      if(imgData.url != fileImgSrc.url){
        fileImgSrc.url = imgData.url
        setFileImgSrc({
          "url":imgData.url
        })
      }
      else if(fileImgSrc.data){
        return <div id="background_back" className={props.className} >
  <div id="background" style={{"--background-url": `url(${fileImgSrc.data ?? ""})`}} />
</div>
      }else if(imgData.data){
        setImgData(imgData.data,imgData.url,imgData.extension,setFileImgSrc)
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