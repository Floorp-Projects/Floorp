function handleRequest(request, response) {
    response.processAsync();
    response.setHeader("Content-Type", "text/event-stream");
    response.write("data: Why so serious?\n\n");
    response.finish();
}
