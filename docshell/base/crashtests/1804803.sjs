function handleRequest(request, response) {
  let counter = Number(getState("load"));
  const reload = counter == 0 ? "self.history.go(0);" : "";
  setState("load", String(++counter));
  const document = `
<script>
    document.addEventListener('DOMContentLoaded', () => {
      ${reload}
      let url = window.location.href;
      for (let i = 0; i < 50; i++) {
        self.history.pushState({x: i}, '', url + "#" + i);
      }
    });
</script>
`;

  response.write(document);
}
