const EXPECTED_CHAIN = [
  "MIIDHjCCAgagAwIBAgIUf1y0AgcMQaVC6jwUyh6Ixiij2hYwDQYJKoZIhvcNAQELBQAwJjEkMCIGA1UEAwwbQWx0ZXJuYXRlIFRydXN0ZWQgQXV0aG9yaXR5MCIYDzIwMTYxMTI3MDAwMDAwWhgPMjAxOTAyMDUwMDAwMDBaMDExLzAtBgNVBAMMJmluY2x1ZGUtc3ViZG9tYWlucy5waW5uaW5nLmV4YW1wbGUuY29tMIIBIjANBgkqhkiG9w0BAQEFAAOCAQ8AMIIBCgKCAQEAwXXGUmYJn3cIKmeR8bh2w39c5TiwbErNIrHL1G+mWtoq3UHIwkmKxKOzwfYUh/QbaYlBvYClHDwSAkTFhKTESDMF5ROMAQbPCL6ahidguuai6PNvI8XZgxO53683g0XazlHU1tzSpss8xwbrzTBw7JjM5AqlkdcpWn9xxb5maR0rLf7ISURZC8Wj6kn9k7HXU0BfF3N2mZWGZiVHl+1CaQiICBFCIGmYikP+5Izmh4HdIramnNKDdRMfkysSjOKG+n0lHAYq0n7wFvGHzdVOgys1uJMPdLqQqovHYWckKrH9bWIUDRjEwLjGj8N0hFcyStfehuZVLx0eGR1xIWjTuwIDAQABozUwMzAxBgNVHREEKjAogiZpbmNsdWRlLXN1YmRvbWFpbnMucGlubmluZy5leGFtcGxlLmNvbTANBgkqhkiG9w0BAQsFAAOCAQEAXn+yS5JdGRm8dqTCG7hUWrlgHL/85FhSTYQ4wt+m90/mb8ARr+XIZtP9GdTloHfZYepc9dRDT9E8wOC7f/nOAQWzyzLqRHrldYNpMAmQIqBmh1tJ/91aKhqbc+lvm1gAOTF5bh5wpEOJBczH0AqDkXP1rX27DI3PyCMT1lqHvFTjVXVOwiZHSOSWRsI89IqAo8Dlfs+y733bJ6/rAmnTK85IHu3Fod7nHviWXkWVDdrbsblvQ6fjMFKS0mj6Vip10++/PwX9rhXyB1seGI+0uXqZ65t3xDZR+yvPtxhc7tbwPdyiUTbVjh43vPHHMCyuK1HqmPrMbAlxZBX3EqkBHQ==",
  "MIIC+zCCAeOgAwIBAgIUb/+pohOlRCuQgMy2GJLCUQq+HeMwDQYJKoZIhvcNAQELBQAwJjEkMCIGA1UEAwwbQWx0ZXJuYXRlIFRydXN0ZWQgQXV0aG9yaXR5MCIYDzIwMTAwMTAxMDAwMDAwWhgPMjA1MDAxMDEwMDAwMDBaMCYxJDAiBgNVBAMMG0FsdGVybmF0ZSBUcnVzdGVkIEF1dGhvcml0eTCCASIwDQYJKoZIhvcNAQEBBQADggEPADCCAQoCggEBAMF1xlJmCZ93CCpnkfG4dsN/XOU4sGxKzSKxy9RvplraKt1ByMJJisSjs8H2FIf0G2mJQb2ApRw8EgJExYSkxEgzBeUTjAEGzwi+moYnYLrmoujzbyPF2YMTud+vN4NF2s5R1Nbc0qbLPMcG680wcOyYzOQKpZHXKVp/ccW+ZmkdKy3+yElEWQvFo+pJ/ZOx11NAXxdzdpmVhmYlR5ftQmkIiAgRQiBpmIpD/uSM5oeB3SK2ppzSg3UTH5MrEozihvp9JRwGKtJ+8Bbxh83VToMrNbiTD3S6kKqLx2FnJCqx/W1iFA0YxMC4xo/DdIRXMkrX3obmVS8dHhkdcSFo07sCAwEAAaMdMBswCwYDVR0PBAQDAgEGMAwGA1UdEwQFMAMBAf8wDQYJKoZIhvcNAQELBQADggEBAAS+qy/sIFV+oia7zsyFhe3Xj3ZHSvmqJ4mxIg5KOPVP2NvDaxD/+pysxGLf69QDRjIsePBdRJz0zZoVl9pSXIn1Kpk0sjzKX2bJtAomog+ZnAZUxtLzoXy/aqaheWm8cRJ8qFOJtSMDRrLISqBXCQLOECqXIxf3Nt3S+Riu2Pam3YymFdtmqUJvLhhekWtEEnXyh/xfAsoUgS3SQ27c4dCYR7XGnFsaXrKXv93QeJmtfvrAZMXEuKaBGPSNHV6QH0S0Loh9Jed2Zp7GxnFtIPYeJ2Q5qtxa8KD/tgGFpAD74eMBdgQ4SxbA/YqqXIt1lLNcr7wm0cPRpP0vIY3hk8k="
  ];

const MOZILLA_PKIX_ERROR_KEY_PINNING_FAILURE = -16384;

function parseReport(request) {
  // read the report from the request
  let inputStream = Components.classes["@mozilla.org/scriptableinputstream;1"].createInstance(Components.interfaces.nsIScriptableInputStream);
  inputStream.init(request.bodyInputStream, 0x01, 0004, 0);

  let body = "";
  if (inputStream) {
    while (inputStream.available()) {
      body = body + inputStream.read(inputStream.available());
    }
  }
  // parse the report
  return JSON.parse(body);
}

function handleRequest(request, response) {
  let report = {};
  let certChain = [];

  switch (request.queryString) {
    case "succeed":
      report = parseReport(request);
      certChain = report.failedCertChain;

      // ensure the cert chain is what we expect
      for (idx in certChain) {
        if (certChain[idx] !== EXPECTED_CHAIN[idx]) {
          // if the chain differs, send an error response to cause test
          // failure
          response.setStatusLine("1.1", 500, "Server error");
          response.write("<html>The report contained an unexpected chain</html>");
          return;
        }
      }

      if (report.errorCode !== MOZILLA_PKIX_ERROR_KEY_PINNING_FAILURE) {
        response.setStatusLine("1.1", 500, "Server error");
        response.write("<html>The report contained an unexpected error code</html>");
        return;
      }

      // if all is as expected, send the 201 the client expects
      response.setStatusLine("1.1", 201, "Created");
      response.write("<html>OK</html>");
      break;
    case "nocert":
      report = parseReport(request);
      certChain = report.failedCertChain;

      if (certChain && certChain.length > 0) {
        // We're not expecting a chain; if there is one, send an error
        response.setStatusLine("1.1", 500, "Server error");
        response.write("<html>The report contained an unexpected chain</html>");
        return;
      }

      // if all is as expected, send the 201 the client expects
      response.setStatusLine("1.1", 201, "Created");
      response.write("<html>OK</html>");
      break;
    case "badcert":
      report = parseReport(request);
      certChain = report.failedCertChain;

      if (!certChain || certChain.length != 2) {
        response.setStatusLine("1.1", 500, "Server error");
        response.write("<html>The report contained an unexpected chain</html>");
        return;
      }

      // if all is as expected, send the 201 the client expects
      response.setStatusLine("1.1", 201, "Created");
      response.write("<html>OK</html>");
      break;
    case "error":
      response.setStatusLine("1.1", 500, "Server error");
      response.write("<html>server error</html>");
      break;
    default:
      response.setStatusLine("1.1", 500, "Server error");
      response.write("<html>succeed, nocert or error expected (got " + request.queryString + ")</html>");
      break;
  }
}
