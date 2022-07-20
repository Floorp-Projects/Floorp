const fixcommentcsstag= document.createElement("style");
      fixcommentcsstag.innerText = `     
.ytd-comment-renderer, yt-formatted-string, #expander, body, #text-content{
  font-family: "BIZ UDPGothic", sans-serif !important;
 }
      `;
document.getElementsByTagName("head")[0].insertAdjacentElement("beforeend",fixcommentcsstag);