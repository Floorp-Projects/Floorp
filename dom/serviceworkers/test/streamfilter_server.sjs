function handleRequest(request, response) {
  const searchParams = new URLSearchParams(request.queryString);

  if (searchParams.get("syntheticResponse") === "0") {
    response.write(String(searchParams));
  }
}
