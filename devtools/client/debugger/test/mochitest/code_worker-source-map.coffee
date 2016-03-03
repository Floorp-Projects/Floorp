# Uses a binary search algorithm to locate a value in the specified array.
binary_search = (items, value) ->

  start = 0
  stop  = items.length - 1
  pivot = Math.floor (start + stop) / 2

  while items[pivot] isnt value and start < stop

    # Adjust the search area.
    stop  = pivot - 1 if value < items[pivot]
    start = pivot + 1 if value > items[pivot]

    # Recalculate the pivot.
    pivot = Math.floor (stop + start) / 2

  # Make sure we've found the correct value.
  if items[pivot] is value then pivot else -1

self.onmessage = (event) ->
  data = event.data
  binary_search(data.items, data.value)
