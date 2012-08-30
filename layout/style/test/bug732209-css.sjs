function handleRequest(request, response)
{
   // First item will be the ID; other items are optional
   var query = request.queryString.split(/&/);

   response.setHeader("Content-Type", "text/css", false);

   if (query.indexOf("cors-anonymous") != -1) {
     response.setHeader("Access-Control-Allow-Origin", "*", false);
   } else if (query.indexOf("cors-credentials") != -1 &&
              request.hasHeader("Origin")) {
     response.setHeader("Access-Control-Allow-Origin",
                        request.getHeader("Origin"), false)
     response.setHeader("Access-Control-Allow-Credentials", "true", false);
   }

   response.write("#" + query[0] + " { color: green !important }" + "\n" +
                  "#" + query[0] + ".reverse { color: red !important }");
}
