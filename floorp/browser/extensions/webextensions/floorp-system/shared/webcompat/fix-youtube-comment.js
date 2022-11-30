const fixcommentcsstag= document.createElement("style");
fixcommentcsstag.innerText = `
.ytd-comment-renderer, yt-formatted-string, #expander, body, #text-content{
  font-family: "Noto Sans JP", sans-serif !important;
 }
`;
document.getElementsByTagName("head")[0].insertAdjacentElement("beforeend",fixcommentcsstag);