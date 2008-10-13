var inputs = document.getElementsByTagName("input");
for (var input, i = 0; input = inputs[i]; ++i)
  if ("submit" == input.type)
    input.click();
