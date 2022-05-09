# Scenario for manual tests


## Drag and drop of tabs partially highlighted in a tree

### Setup

* Prepare tabs as:
  - A
    - B
      - C
    - D
    - E

### Test

1. Activate B.
2. Ctrl-Click on D.
3. Drag B (and D) above A.
   * Expected result: tree becomes:
     - B
     - D
     - A
       - C
       - E

