// force the Browser to Show a Download Prompt

function handleRequest(request, response)
{
  response.setHeader("Cache-Control", "no-cache", false);
  response.setHeader("Content-Disposition", "attachment");
  response.setHeader("Content-Type", "application/octet-stream");
  response.write('🙈🙊🐵🙊');
}
