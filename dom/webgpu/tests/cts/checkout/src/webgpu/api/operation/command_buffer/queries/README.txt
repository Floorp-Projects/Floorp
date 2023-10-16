TODO: test the behavior of creating/using/resolving queries.
- occlusion
- pipeline statistics
  TODO: pipeline statistics queries are removed from core; consider moving tests to another suite.
- timestamp
- nested (e.g. timestamp or PS query inside occlusion query), if any such cases are valid. Try
  writing to the same query set (at same or different indices), if valid. Check results make sense.
- start a query (all types) with no draw calls
